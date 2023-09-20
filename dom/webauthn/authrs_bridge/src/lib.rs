/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
extern crate log;

#[macro_use]
extern crate xpcom;

use authenticator::{
    authenticatorservice::{RegisterArgs, SignArgs},
    crypto::COSEKeyType,
    ctap2::attestation::AttestationObject,
    ctap2::commands::get_info::AuthenticatorVersion,
    ctap2::server::{
        AuthenticationExtensionsClientInputs, PublicKeyCredentialDescriptor,
        PublicKeyCredentialParameters, RelyingParty, ResidentKeyRequirement, User,
        UserVerificationRequirement,
    },
    errors::{AuthenticatorError, PinError, U2FTokenError},
    statecallback::StateCallback,
    Assertion, Pin, RegisterResult, SignResult, StateMachine, StatusPinUv, StatusUpdate,
};
use base64::Engine;
use moz_task::RunnableBuilder;
use nserror::{
    nsresult, NS_ERROR_DOM_INVALID_STATE_ERR, NS_ERROR_DOM_NOT_ALLOWED_ERR,
    NS_ERROR_DOM_NOT_SUPPORTED_ERR, NS_ERROR_DOM_UNKNOWN_ERR, NS_ERROR_FAILURE,
    NS_ERROR_INVALID_ARG, NS_ERROR_NOT_AVAILABLE, NS_ERROR_NOT_IMPLEMENTED, NS_ERROR_NULL_POINTER,
    NS_OK,
};
use nsstring::{nsACString, nsCString, nsString};
use serde_cbor;
use std::cell::RefCell;
use std::sync::mpsc::{channel, Receiver, RecvError, Sender};
use std::sync::{Arc, Mutex};
use thin_vec::{thin_vec, ThinVec};
use xpcom::interfaces::{
    nsICredentialParameters, nsICtapRegisterArgs, nsICtapRegisterResult, nsICtapSignArgs,
    nsICtapSignResult, nsIWebAuthnAttObj, nsIWebAuthnController, nsIWebAuthnTransport,
};
use xpcom::{xpcom_method, RefPtr};

mod test_token;
use test_token::TestTokenManager;

fn make_prompt(action: &str, tid: u64, origin: &str, browsing_context_id: u64) -> String {
    format!(
        r#"{{"action":"{action}","tid":{tid},"origin":"{origin}","browsingContextId":{browsing_context_id}}}"#,
    )
}

fn make_uv_invalid_error_prompt(
    tid: u64,
    origin: &str,
    browsing_context_id: u64,
    retries: i64,
) -> String {
    format!(
        r#"{{"action":"uv-invalid","tid":{tid},"origin":"{origin}","browsingContextId":{browsing_context_id},"retriesLeft":{retries}}}"#,
    )
}

fn make_pin_required_prompt(
    tid: u64,
    origin: &str,
    browsing_context_id: u64,
    was_invalid: bool,
    retries: i64,
) -> String {
    format!(
        r#"{{"action":"pin-required","tid":{tid},"origin":"{origin}","browsingContextId":{browsing_context_id},"wasInvalid":{was_invalid},"retriesLeft":{retries}}}"#,
    )
}

fn authrs_to_nserror(e: &AuthenticatorError) -> nsresult {
    match e {
        AuthenticatorError::U2FToken(U2FTokenError::NotSupported) => NS_ERROR_DOM_NOT_SUPPORTED_ERR,
        AuthenticatorError::U2FToken(U2FTokenError::InvalidState) => NS_ERROR_DOM_INVALID_STATE_ERR,
        AuthenticatorError::U2FToken(U2FTokenError::NotAllowed) => NS_ERROR_DOM_NOT_ALLOWED_ERR,
        AuthenticatorError::PinError(PinError::PinRequired) => NS_ERROR_DOM_INVALID_STATE_ERR,
        AuthenticatorError::PinError(PinError::InvalidPin(_)) => NS_ERROR_DOM_INVALID_STATE_ERR,
        AuthenticatorError::PinError(PinError::PinAuthBlocked) => NS_ERROR_DOM_INVALID_STATE_ERR,
        AuthenticatorError::PinError(PinError::PinBlocked) => NS_ERROR_DOM_INVALID_STATE_ERR,
        AuthenticatorError::PinError(PinError::PinNotSet) => NS_ERROR_DOM_INVALID_STATE_ERR,
        AuthenticatorError::CredentialExcluded => NS_ERROR_DOM_INVALID_STATE_ERR,
        _ => NS_ERROR_DOM_UNKNOWN_ERR,
    }
}

#[xpcom(implement(nsICtapRegisterResult), atomic)]
pub struct CtapRegisterResult {
    result: Result<RegisterResult, AuthenticatorError>,
}

impl CtapRegisterResult {
    xpcom_method!(get_attestation_object => GetAttestationObject() -> ThinVec<u8>);
    fn get_attestation_object(&self) -> Result<ThinVec<u8>, nsresult> {
        let mut out = ThinVec::new();
        let make_cred_res = self.result.as_ref().or(Err(NS_ERROR_FAILURE))?;
        serde_cbor::to_writer(&mut out, &make_cred_res.att_obj).or(Err(NS_ERROR_FAILURE))?;
        Ok(out)
    }

    xpcom_method!(get_credential_id => GetCredentialId() -> ThinVec<u8>);
    fn get_credential_id(&self) -> Result<ThinVec<u8>, nsresult> {
        let mut out = ThinVec::new();
        if let Ok(make_cred_res) = &self.result {
            if let Some(credential_data) = &make_cred_res.att_obj.auth_data.credential_data {
                out.extend_from_slice(&credential_data.credential_id);
                return Ok(out);
            }
        }
        Err(NS_ERROR_FAILURE)
    }

    xpcom_method!(get_transports => GetTransports() -> ThinVec<nsString>);
    fn get_transports(&self) -> Result<ThinVec<nsString>, nsresult> {
        if self.result.is_err() {
            return Err(NS_ERROR_FAILURE);
        }
        // The list that we return here might be included in a future GetAssertion request as a
        // hint as to which transports to try. We currently only support the USB transport. If
        // that changes, we will need a mechanism to track which transport was used for a
        // request.
        Ok(thin_vec![nsString::from("usb")])
    }

    xpcom_method!(get_cred_props_rk => GetCredPropsRk() -> bool);
    fn get_cred_props_rk(&self) -> Result<bool, nsresult> {
        let result = self.result.as_ref().or(Err(NS_ERROR_FAILURE))?;
        let cred_props = result
            .extensions
            .cred_props
            .as_ref()
            .ok_or(NS_ERROR_NOT_AVAILABLE)?;
        Ok(cred_props.rk)
    }

    xpcom_method!(get_status => GetStatus() -> nsresult);
    fn get_status(&self) -> Result<nsresult, nsresult> {
        match &self.result {
            Ok(_) => Ok(NS_OK),
            Err(e) => Ok(authrs_to_nserror(e)),
        }
    }
}

#[xpcom(implement(nsIWebAuthnAttObj), atomic)]
pub struct WebAuthnAttObj {
    att_obj: AttestationObject,
}

impl WebAuthnAttObj {
    xpcom_method!(get_attestation_object => GetAttestationObject() -> ThinVec<u8>);
    fn get_attestation_object(&self) -> Result<ThinVec<u8>, nsresult> {
        let mut out = ThinVec::new();
        serde_cbor::to_writer(&mut out, &self.att_obj).or(Err(NS_ERROR_FAILURE))?;
        Ok(out)
    }

    xpcom_method!(get_authenticator_data => GetAuthenticatorData() -> ThinVec<u8>);
    fn get_authenticator_data(&self) -> Result<ThinVec<u8>, nsresult> {
        // TODO(https://github.com/mozilla/authenticator-rs/issues/302) use to_writer
        Ok(self.att_obj.auth_data.to_vec().into())
    }

    xpcom_method!(get_public_key => GetPublicKey() -> ThinVec<u8>);
    fn get_public_key(&self) -> Result<ThinVec<u8>, nsresult> {
        let Some(credential_data) = &self.att_obj.auth_data.credential_data else {
            return Err(NS_ERROR_FAILURE);
        };
        // We only support encoding (some) EC2 keys in DER SPKI format.
        let COSEKeyType::EC2(ref key) = credential_data.credential_public_key.key else {
            return Err(NS_ERROR_NOT_AVAILABLE);
        };
        Ok(key.der_spki().or(Err(NS_ERROR_NOT_AVAILABLE))?.into())
    }

    xpcom_method!(get_public_key_algorithm => GetPublicKeyAlgorithm() -> i32);
    fn get_public_key_algorithm(&self) -> Result<i32, nsresult> {
        if let Some(credential_data) = &self.att_obj.auth_data.credential_data {
            // safe to cast to i32 by inspection of defined values
            Ok(credential_data.credential_public_key.alg as i32)
        } else {
            Err(NS_ERROR_FAILURE)
        }
    }
}

#[xpcom(implement(nsICtapSignResult), atomic)]
pub struct CtapSignResult {
    result: Result<Assertion, AuthenticatorError>,
}

impl CtapSignResult {
    xpcom_method!(get_credential_id => GetCredentialId() -> ThinVec<u8>);
    fn get_credential_id(&self) -> Result<ThinVec<u8>, nsresult> {
        let mut out = ThinVec::new();
        if let Ok(assertion) = &self.result {
            if let Some(cred) = &assertion.credentials {
                out.extend_from_slice(&cred.id);
                return Ok(out);
            }
        }
        Err(NS_ERROR_FAILURE)
    }

    xpcom_method!(get_signature => GetSignature() -> ThinVec<u8>);
    fn get_signature(&self) -> Result<ThinVec<u8>, nsresult> {
        let mut out = ThinVec::new();
        if let Ok(assertion) = &self.result {
            out.extend_from_slice(&assertion.signature);
            return Ok(out);
        }
        Err(NS_ERROR_FAILURE)
    }

    xpcom_method!(get_authenticator_data => GetAuthenticatorData() -> ThinVec<u8>);
    fn get_authenticator_data(&self) -> Result<ThinVec<u8>, nsresult> {
        self.result
            .as_ref()
            .map(|assertion| assertion.auth_data.to_vec().into())
            .or(Err(NS_ERROR_FAILURE))
    }

    xpcom_method!(get_user_handle => GetUserHandle() -> ThinVec<u8>);
    fn get_user_handle(&self) -> Result<ThinVec<u8>, nsresult> {
        let mut out = ThinVec::new();
        if let Ok(assertion) = &self.result {
            if let Some(user) = &assertion.user {
                out.extend_from_slice(&user.id);
                return Ok(out);
            }
        }
        Err(NS_ERROR_FAILURE)
    }

    xpcom_method!(get_user_name => GetUserName() -> nsACString);
    fn get_user_name(&self) -> Result<nsCString, nsresult> {
        if let Ok(assertion) = &self.result {
            if let Some(user) = &assertion.user {
                if let Some(name) = &user.name {
                    return Ok(nsCString::from(name));
                }
            }
        }
        Err(NS_ERROR_NOT_AVAILABLE)
    }

    xpcom_method!(get_rp_id_hash => GetRpIdHash() -> ThinVec<u8>);
    fn get_rp_id_hash(&self) -> Result<ThinVec<u8>, nsresult> {
        // assertion.auth_data.rp_id_hash
        let mut out = ThinVec::new();
        if let Ok(assertion) = &self.result {
            out.extend_from_slice(&assertion.auth_data.rp_id_hash.0);
            return Ok(out);
        }
        Err(NS_ERROR_FAILURE)
    }

    xpcom_method!(get_status => GetStatus() -> nsresult);
    fn get_status(&self) -> Result<nsresult, nsresult> {
        match &self.result {
            Ok(_) => Ok(NS_OK),
            Err(e) => Ok(authrs_to_nserror(e)),
        }
    }
}

/// Controller wraps a raw pointer to an nsIWebAuthnController. The AuthrsTransport struct holds a
/// Controller which we need to initialize from the SetController XPCOM method.  Since XPCOM
/// methods take &self, Controller must have interior mutability.
#[derive(Clone)]
struct Controller(RefCell<*const nsIWebAuthnController>);

/// Our implementation of nsIWebAuthnController in WebAuthnController.cpp has the property that its
/// XPCOM methods are safe to call from any thread, hence a raw pointer to an nsIWebAuthnController
/// is Send.
unsafe impl Send for Controller {}

impl Controller {
    fn init(&self, ptr: *const nsIWebAuthnController) -> Result<(), nsresult> {
        self.0.replace(ptr);
        Ok(())
    }

    fn send_prompt(&self, tid: u64, msg: &str) {
        if (*self.0.borrow()).is_null() {
            warn!("Controller not initialized");
            return;
        }
        let notification_str = nsCString::from(msg);
        unsafe {
            (**(self.0.borrow())).SendPromptNotificationPreformatted(tid, &*notification_str);
        }
    }

    fn finish_register(
        &self,
        tid: u64,
        result: Result<RegisterResult, AuthenticatorError>,
    ) -> Result<(), nsresult> {
        if (*self.0.borrow()).is_null() {
            return Err(NS_ERROR_FAILURE);
        }
        let wrapped_result = CtapRegisterResult::allocate(InitCtapRegisterResult { result })
            .query_interface::<nsICtapRegisterResult>()
            .ok_or(NS_ERROR_FAILURE)?;
        unsafe {
            (**(self.0.borrow())).FinishRegister(tid, wrapped_result.coerce());
        }
        Ok(())
    }

    fn finish_sign(
        &self,
        tid: u64,
        result: Result<SignResult, AuthenticatorError>,
    ) -> Result<(), nsresult> {
        if (*self.0.borrow()).is_null() {
            return Err(NS_ERROR_FAILURE);
        }

        // If result is an error, we return a single CtapSignResult that has its status field set
        // to an error. Otherwise we convert the entries of SignResult (= Vec<Assertion>) into
        // CtapSignResults with OK statuses.
        let mut assertions: ThinVec<Option<RefPtr<nsICtapSignResult>>> = ThinVec::new();
        match result {
            Err(e) => assertions.push(
                CtapSignResult::allocate(InitCtapSignResult { result: Err(e) })
                    .query_interface::<nsICtapSignResult>(),
            ),
            Ok(result) => {
                for assertion in result.assertions {
                    assertions.push(
                        CtapSignResult::allocate(InitCtapSignResult {
                            result: Ok(assertion),
                        })
                        .query_interface::<nsICtapSignResult>(),
                    );
                }
            }
        }

        unsafe {
            (**(self.0.borrow())).FinishSign(tid, &mut assertions);
        }
        Ok(())
    }
}

// The state machine creates a Sender<Pin>/Receiver<Pin> channel in ask_user_for_pin. It passes the
// Sender through status_callback, which stores the Sender in the pin_receiver field of an
// AuthrsTransport. The u64 in PinReceiver is a transaction ID, which the AuthrsTransport uses the
// transaction ID as a consistency check.
type PinReceiver = Option<(u64, Sender<Pin>)>;

fn status_callback(
    status_rx: Receiver<StatusUpdate>,
    tid: u64,
    origin: &String,
    browsing_context_id: u64,
    controller: Controller,
    pin_receiver: Arc<Mutex<PinReceiver>>, /* Shared with an AuthrsTransport */
) {
    loop {
        match status_rx.recv() {
            Ok(StatusUpdate::SelectDeviceNotice) => {
                debug!("STATUS: Please select a device by touching one of them.");
                let notification_str =
                    make_prompt("select-device", tid, origin, browsing_context_id);
                controller.send_prompt(tid, &notification_str);
            }
            Ok(StatusUpdate::PresenceRequired) => {
                debug!("STATUS: Waiting for user presence");
                let notification_str = make_prompt("presence", tid, origin, browsing_context_id);
                controller.send_prompt(tid, &notification_str);
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::PinRequired(sender))) => {
                let guard = pin_receiver.lock();
                if let Ok(mut entry) = guard {
                    entry.replace((tid, sender));
                } else {
                    return;
                }
                let notification_str =
                    make_pin_required_prompt(tid, origin, browsing_context_id, false, -1);
                controller.send_prompt(tid, &notification_str);
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::InvalidPin(sender, attempts))) => {
                let guard = pin_receiver.lock();
                if let Ok(mut entry) = guard {
                    entry.replace((tid, sender));
                } else {
                    return;
                }
                let notification_str = make_pin_required_prompt(
                    tid,
                    origin,
                    browsing_context_id,
                    true,
                    attempts.map_or(-1, |x| x as i64),
                );
                controller.send_prompt(tid, &notification_str);
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::PinAuthBlocked)) => {
                let notification_str =
                    make_prompt("pin-auth-blocked", tid, origin, browsing_context_id);
                controller.send_prompt(tid, &notification_str);
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::PinBlocked)) => {
                let notification_str =
                    make_prompt("device-blocked", tid, origin, browsing_context_id);
                controller.send_prompt(tid, &notification_str);
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::PinNotSet)) => {
                let notification_str = make_prompt("pin-not-set", tid, origin, browsing_context_id);
                controller.send_prompt(tid, &notification_str);
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::InvalidUv(attempts))) => {
                let notification_str = make_uv_invalid_error_prompt(
                    tid,
                    origin,
                    browsing_context_id,
                    attempts.map_or(-1, |x| x as i64),
                );
                controller.send_prompt(tid, &notification_str);
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::UvBlocked)) => {
                let notification_str = make_prompt("uv-blocked", tid, origin, browsing_context_id);
                controller.send_prompt(tid, &notification_str);
            }
            Ok(StatusUpdate::PinUvError(StatusPinUv::PinIsTooShort))
            | Ok(StatusUpdate::PinUvError(StatusPinUv::PinIsTooLong(..))) => {
                // These should never happen.
                warn!("STATUS: Got unexpected StatusPinUv-error.");
            }
            Ok(StatusUpdate::InteractiveManagement(_)) => {
                debug!("STATUS: interactive management");
            }
            Err(RecvError) => {
                debug!("STATUS: end");
                return;
            }
        }
    }
}

// AuthrsTransport provides an nsIWebAuthnTransport interface to an AuthenticatorService. This
// allows an nsIWebAuthnController to dispatch MakeCredential and GetAssertion requests to USB HID
// tokens. The AuthrsTransport struct also keeps 1) a pointer to the active nsIWebAuthnController,
// which is used to prompt the user for input and to return results to the controller; and
// 2) a channel through which to receive a pin callback.
#[xpcom(implement(nsIWebAuthnTransport), atomic)]
pub struct AuthrsTransport {
    usb_token_manager: RefCell<StateMachine>, // interior mutable for use in XPCOM methods
    test_token_manager: TestTokenManager,
    controller: Controller,
    pin_receiver: Arc<Mutex<PinReceiver>>,
}

impl AuthrsTransport {
    xpcom_method!(get_controller => GetController() -> *const nsIWebAuthnController);
    fn get_controller(&self) -> Result<RefPtr<nsIWebAuthnController>, nsresult> {
        Err(NS_ERROR_NOT_IMPLEMENTED)
    }

    // # Safety
    //
    // This will mutably borrow the controller pointer through a RefCell. The caller must ensure
    // that at most one WebAuthn transaction is active at any given time.
    xpcom_method!(set_controller => SetController(aController: *const nsIWebAuthnController));
    fn set_controller(&self, controller: *const nsIWebAuthnController) -> Result<(), nsresult> {
        self.controller.init(controller)
    }

    xpcom_method!(pin_callback => PinCallback(aTransactionId: u64, aPin: *const nsACString));
    fn pin_callback(&self, transaction_id: u64, pin: &nsACString) -> Result<(), nsresult> {
        let mut guard = self.pin_receiver.lock().or(Err(NS_ERROR_FAILURE))?;
        match guard.take() {
            // The pin_receiver is single-use.
            Some((tid, channel)) if tid == transaction_id => channel
                .send(Pin::new(&pin.to_string()))
                .or(Err(NS_ERROR_FAILURE)),
            // Either we weren't expecting a pin, or the controller is confused
            // about which transaction is active. Neither is recoverable, so it's
            // OK to drop the PinReceiver here.
            _ => Err(NS_ERROR_FAILURE),
        }
    }

    // # Safety
    //
    // This will mutably borrow usb_token_manager through a RefCell. The caller must ensure that at
    // most one WebAuthn transaction is active at any given time.
    xpcom_method!(make_credential => MakeCredential(aTid: u64, aBrowsingContextId: u64, aArgs: *const nsICtapRegisterArgs));
    fn make_credential(
        &self,
        tid: u64,
        browsing_context_id: u64,
        args: *const nsICtapRegisterArgs,
    ) -> Result<(), nsresult> {
        if args.is_null() {
            return Err(NS_ERROR_NULL_POINTER);
        }
        let args = unsafe { &*args };

        let mut origin = nsString::new();
        unsafe { args.GetOrigin(&mut *origin) }.to_result()?;

        let mut relying_party_id = nsString::new();
        unsafe { args.GetRpId(&mut *relying_party_id) }.to_result()?;

        let mut client_data_hash = ThinVec::new();
        unsafe { args.GetClientDataHash(&mut client_data_hash) }.to_result()?;
        let mut client_data_hash_arr = [0u8; 32];
        client_data_hash_arr.copy_from_slice(&client_data_hash);

        let mut timeout_ms = 0u32;
        unsafe { args.GetTimeoutMS(&mut timeout_ms) }.to_result()?;

        let mut exclude_list = ThinVec::new();
        unsafe { args.GetExcludeList(&mut exclude_list) }.to_result()?;
        let exclude_list = exclude_list
            .iter_mut()
            .map(|id| PublicKeyCredentialDescriptor {
                id: id.to_vec(),
                transports: vec![],
            })
            .collect();

        let mut relying_party_name = nsString::new();
        unsafe { args.GetRpName(&mut *relying_party_name) }.to_result()?;

        let mut user_id = ThinVec::new();
        unsafe { args.GetUserId(&mut user_id) }.to_result()?;

        let mut user_name = nsString::new();
        unsafe { args.GetUserName(&mut *user_name) }.to_result()?;

        let mut user_display_name = nsString::new();
        unsafe { args.GetUserDisplayName(&mut *user_display_name) }.to_result()?;

        let mut cose_algs = ThinVec::new();
        unsafe { args.GetCoseAlgs(&mut cose_algs) }.to_result()?;
        let pub_cred_params = cose_algs
            .iter()
            .filter_map(|alg| PublicKeyCredentialParameters::try_from(*alg).ok())
            .collect();

        let mut resident_key = nsString::new();
        unsafe { args.GetResidentKey(&mut *resident_key) }.to_result()?;
        let resident_key_req = if resident_key.eq("required") {
            ResidentKeyRequirement::Required
        } else if resident_key.eq("preferred") {
            ResidentKeyRequirement::Preferred
        } else if resident_key.eq("discouraged") {
            ResidentKeyRequirement::Discouraged
        } else {
            return Err(NS_ERROR_FAILURE);
        };

        let mut user_verification = nsString::new();
        unsafe { args.GetUserVerification(&mut *user_verification) }.to_result()?;
        let user_verification_req = if user_verification.eq("required") {
            UserVerificationRequirement::Required
        } else if user_verification.eq("discouraged") {
            UserVerificationRequirement::Discouraged
        } else {
            UserVerificationRequirement::Preferred
        };

        let mut authenticator_attachment = nsString::new();
        if unsafe { args.GetAuthenticatorAttachment(&mut *authenticator_attachment) }
            .to_result()
            .is_ok()
        {
            if authenticator_attachment.eq("platform") {
                return Err(NS_ERROR_FAILURE);
            }
        }

        let mut attestation_conveyance_preference = nsString::new();
        unsafe { args.GetAttestationConveyancePreference(&mut *attestation_conveyance_preference) }
            .to_result()?;
        let none_attestation = attestation_conveyance_preference.eq("none");

        let mut cred_props = false;
        unsafe { args.GetCredProps(&mut cred_props) }.to_result()?;

        // TODO(Bug 1593571) - Add this to the extensions
        // let mut hmac_create_secret = None;
        // let mut maybe_hmac_create_secret = false;
        // match unsafe { args.GetHmacCreateSecret(&mut maybe_hmac_create_secret) }.to_result() {
        //     Ok(_) => hmac_create_secret = Some(maybe_hmac_create_secret),
        //     _ => (),
        // }

        let info = RegisterArgs {
            client_data_hash: client_data_hash_arr,
            relying_party: RelyingParty {
                id: relying_party_id.to_string(),
                name: None,
            },
            origin: origin.to_string(),
            user: User {
                id: user_id.to_vec(),
                name: Some(user_name.to_string()),
                display_name: None,
            },
            pub_cred_params,
            exclude_list,
            user_verification_req,
            resident_key_req,
            extensions: AuthenticationExtensionsClientInputs {
                cred_props: Some(cred_props),
                ..Default::default()
            },
            pin: None,
            use_ctap1_fallback: !static_prefs::pref!("security.webauthn.ctap2"),
        };

        let (status_tx, status_rx) = channel::<StatusUpdate>();
        let pin_receiver = self.pin_receiver.clone();
        let controller = self.controller.clone();
        let status_origin = origin.to_string();
        RunnableBuilder::new(
            "AuthrsTransport::MakeCredential::StatusReceiver",
            move || {
                status_callback(
                    status_rx,
                    tid,
                    &status_origin,
                    browsing_context_id,
                    controller,
                    pin_receiver,
                )
            },
        )
        .may_block(true)
        .dispatch_background_task()?;

        let controller = self.controller.clone();
        let callback_origin = origin.to_string();
        let state_callback = StateCallback::<Result<RegisterResult, AuthenticatorError>>::new(
            Box::new(move |result| {
                let result = match result {
                    Ok(mut make_cred_res) => {
                        // Tokens always provide attestation, but the user may have asked we not
                        // include the attestation statement in the response.
                        if none_attestation {
                            make_cred_res.att_obj.anonymize();
                        }
                        Ok(make_cred_res)
                    }
                    Err(e @ AuthenticatorError::CredentialExcluded) => {
                        let notification_str = make_prompt(
                            "already-registered",
                            tid,
                            &callback_origin,
                            browsing_context_id,
                        );
                        controller.send_prompt(tid, &notification_str);
                        Err(e)
                    }
                    Err(e) => Err(e),
                };
                let _ = controller.finish_register(tid, result);
            }),
        );

        // The authenticator crate provides an `AuthenticatorService` which can dispatch a request
        // in parallel to any number of transports. We only support the USB transport in production
        // configurations, so we do not need the full generality of `AuthenticatorService` here.
        // We disable the USB transport in tests that use virtual devices.
        if static_prefs::pref!("security.webauth.webauthn_enable_usbtoken") {
            self.usb_token_manager.borrow_mut().register(
                timeout_ms as u64,
                info.into(),
                status_tx,
                state_callback,
            );
        } else if static_prefs::pref!("security.webauth.webauthn_enable_softtoken") {
            self.test_token_manager.register(
                timeout_ms as u64,
                info.into(),
                status_tx,
                state_callback,
            );
        } else {
            return Err(NS_ERROR_FAILURE);
        }

        Ok(())
    }

    // # Safety
    //
    // This will mutably borrow usb_token_manager through a RefCell. The caller must ensure that at
    // most one WebAuthn transaction is active at any given time.
    xpcom_method!(get_assertion => GetAssertion(aTid: u64, aBrowsingContextId: u64, aArgs: *const nsICtapSignArgs));
    fn get_assertion(
        &self,
        tid: u64,
        browsing_context_id: u64,
        args: *const nsICtapSignArgs,
    ) -> Result<(), nsresult> {
        if args.is_null() {
            return Err(NS_ERROR_NULL_POINTER);
        }
        let args = unsafe { &*args };

        let mut origin = nsString::new();
        unsafe { args.GetOrigin(&mut *origin) }.to_result()?;

        let mut relying_party_id = nsString::new();
        unsafe { args.GetRpId(&mut *relying_party_id) }.to_result()?;

        let mut client_data_hash = ThinVec::new();
        unsafe { args.GetClientDataHash(&mut client_data_hash) }.to_result()?;
        let mut client_data_hash_arr = [0u8; 32];
        client_data_hash_arr.copy_from_slice(&client_data_hash);

        let mut timeout_ms = 0u32;
        unsafe { args.GetTimeoutMS(&mut timeout_ms) }.to_result()?;

        let mut allow_list = ThinVec::new();
        unsafe { args.GetAllowList(&mut allow_list) }.to_result()?;
        let allow_list: Vec<_> = allow_list
            .iter_mut()
            .map(|id| PublicKeyCredentialDescriptor {
                id: id.to_vec(),
                transports: vec![],
            })
            .collect();

        let mut user_verification = nsString::new();
        unsafe { args.GetUserVerification(&mut *user_verification) }.to_result()?;
        let user_verification_req = if user_verification.eq("required") {
            UserVerificationRequirement::Required
        } else if user_verification.eq("discouraged") {
            UserVerificationRequirement::Discouraged
        } else {
            UserVerificationRequirement::Preferred
        };

        let mut app_id = None;
        let mut maybe_app_id = nsString::new();
        match unsafe { args.GetAppId(&mut *maybe_app_id) }.to_result() {
            Ok(_) => app_id = Some(maybe_app_id.to_string()),
            _ => (),
        }

        let (status_tx, status_rx) = channel::<StatusUpdate>();
        let pin_receiver = self.pin_receiver.clone();
        let controller = self.controller.clone();
        let status_origin = origin.to_string();
        RunnableBuilder::new("AuthrsTransport::GetAssertion::StatusReceiver", move || {
            status_callback(
                status_rx,
                tid,
                &status_origin,
                browsing_context_id,
                controller,
                pin_receiver,
            )
        })
        .may_block(true)
        .dispatch_background_task()?;

        let uniq_allowed_cred = if allow_list.len() == 1 {
            allow_list.first().cloned()
        } else {
            None
        };

        let controller = self.controller.clone();
        let state_callback = StateCallback::<Result<SignResult, AuthenticatorError>>::new(
            Box::new(move |mut result| {
                if uniq_allowed_cred.is_some() {
                    // In CTAP 2.0, but not CTAP 2.1, the assertion object's credential field
                    // "May be omitted if the allowList has exactly one credential." If we had
                    // a unique allowed credential, then copy its descriptor to the output.
                    if let Ok(Some(assertion)) =
                        result.as_mut().map(|result| result.assertions.first_mut())
                    {
                        assertion.credentials = uniq_allowed_cred;
                    }
                }
                let _ = controller.finish_sign(tid, result);
            }),
        );

        let info = SignArgs {
            client_data_hash: client_data_hash_arr,
            relying_party_id: relying_party_id.to_string(),
            origin: origin.to_string(),
            allow_list,
            user_verification_req,
            user_presence_req: true,
            extensions: AuthenticationExtensionsClientInputs {
                app_id,
                ..Default::default()
            },
            pin: None,
            use_ctap1_fallback: !static_prefs::pref!("security.webauthn.ctap2"),
        };

        // As in `register`, we are intentionally avoiding `AuthenticatorService` here.
        if static_prefs::pref!("security.webauth.webauthn_enable_usbtoken") {
            self.usb_token_manager.borrow_mut().sign(
                timeout_ms as u64,
                info.into(),
                status_tx,
                state_callback,
            );
        } else if static_prefs::pref!("security.webauth.webauthn_enable_softtoken") {
            self.test_token_manager
                .sign(timeout_ms as u64, info.into(), status_tx, state_callback);
        } else {
            return Err(NS_ERROR_FAILURE);
        }

        Ok(())
    }

    // # Safety
    //
    // This will mutably borrow usb_token_manager through a RefCell. The caller must ensure that at
    // most one WebAuthn transaction is active at any given time.
    xpcom_method!(cancel => Cancel());
    fn cancel(&self) -> Result<(), nsresult> {
        // We may be waiting for a pin. Drop the channel to release the
        // state machine from `ask_user_for_pin`.
        drop(self.pin_receiver.lock().or(Err(NS_ERROR_FAILURE))?.take());

        self.usb_token_manager.borrow_mut().cancel();

        Ok(())
    }

    xpcom_method!(
        add_virtual_authenticator => AddVirtualAuthenticator(
            protocol: *const nsACString,
            transport: *const nsACString,
            hasResidentKey: bool,
            hasUserVerification: bool,
            isUserConsenting: bool,
            isUserVerified: bool) -> u64
    );
    fn add_virtual_authenticator(
        &self,
        protocol: &nsACString,
        transport: &nsACString,
        has_resident_key: bool,
        has_user_verification: bool,
        is_user_consenting: bool,
        is_user_verified: bool,
    ) -> Result<u64, nsresult> {
        let protocol = match protocol.to_string().as_str() {
            "ctap1/u2f" => AuthenticatorVersion::U2F_V2,
            "ctap2" => AuthenticatorVersion::FIDO_2_0,
            "ctap2_1" => AuthenticatorVersion::FIDO_2_1,
            _ => return Err(NS_ERROR_INVALID_ARG),
        };
        match transport.to_string().as_str() {
            "usb" | "nfc" | "ble" | "smart-card" | "hybrid" | "internal" => (),
            _ => return Err(NS_ERROR_INVALID_ARG),
        };
        self.test_token_manager.add_virtual_authenticator(
            protocol,
            has_resident_key,
            has_user_verification,
            is_user_consenting,
            is_user_verified,
        )
    }

    xpcom_method!(remove_virtual_authenticator => RemoveVirtualAuthenticator(authenticatorId: u64));
    fn remove_virtual_authenticator(&self, authenticator_id: u64) -> Result<(), nsresult> {
        self.test_token_manager
            .remove_virtual_authenticator(authenticator_id)
    }

    xpcom_method!(
        add_credential => AddCredential(
            authenticatorId: u64,
            credentialId: *const nsACString,
            isResidentCredential: bool,
            rpId: *const nsACString,
            privateKey: *const nsACString,
            userHandle: *const nsACString,
            signCount: u32)
    );
    fn add_credential(
        &self,
        authenticator_id: u64,
        credential_id: &nsACString,
        is_resident_credential: bool,
        rp_id: &nsACString,
        private_key: &nsACString,
        user_handle: &nsACString,
        sign_count: u32,
    ) -> Result<(), nsresult> {
        let credential_id = base64::engine::general_purpose::URL_SAFE_NO_PAD
            .decode(credential_id)
            .or(Err(NS_ERROR_INVALID_ARG))?;
        let private_key = base64::engine::general_purpose::URL_SAFE_NO_PAD
            .decode(private_key)
            .or(Err(NS_ERROR_INVALID_ARG))?;
        let user_handle = base64::engine::general_purpose::URL_SAFE_NO_PAD
            .decode(user_handle)
            .or(Err(NS_ERROR_INVALID_ARG))?;
        self.test_token_manager.add_credential(
            authenticator_id,
            &credential_id,
            &private_key,
            &user_handle,
            sign_count,
            rp_id.to_string(),
            is_resident_credential,
        )
    }

    xpcom_method!(get_credentials => GetCredentials(authenticatorId: u64) -> ThinVec<Option<RefPtr<nsICredentialParameters>>>);
    fn get_credentials(
        &self,
        authenticator_id: u64,
    ) -> Result<ThinVec<Option<RefPtr<nsICredentialParameters>>>, nsresult> {
        self.test_token_manager.get_credentials(authenticator_id)
    }

    xpcom_method!(remove_credential => RemoveCredential(authenticatorId: u64, credentialId: *const nsACString));
    fn remove_credential(
        &self,
        authenticator_id: u64,
        credential_id: &nsACString,
    ) -> Result<(), nsresult> {
        let credential_id = base64::engine::general_purpose::URL_SAFE_NO_PAD
            .decode(credential_id)
            .or(Err(NS_ERROR_INVALID_ARG))?;
        self.test_token_manager
            .remove_credential(authenticator_id, credential_id.as_ref())
    }

    xpcom_method!(remove_all_credentials => RemoveAllCredentials(authenticatorId: u64));
    fn remove_all_credentials(&self, authenticator_id: u64) -> Result<(), nsresult> {
        self.test_token_manager
            .remove_all_credentials(authenticator_id)
    }

    xpcom_method!(set_user_verified => SetUserVerified(authenticatorId: u64, isUserVerified: bool));
    fn set_user_verified(
        &self,
        authenticator_id: u64,
        is_user_verified: bool,
    ) -> Result<(), nsresult> {
        self.test_token_manager
            .set_user_verified(authenticator_id, is_user_verified)
    }
}

#[no_mangle]
pub extern "C" fn authrs_transport_constructor(
    result: *mut *const nsIWebAuthnTransport,
) -> nsresult {
    let wrapper = AuthrsTransport::allocate(InitAuthrsTransport {
        usb_token_manager: RefCell::new(StateMachine::new()),
        test_token_manager: TestTokenManager::new(),
        controller: Controller(RefCell::new(std::ptr::null())),
        pin_receiver: Arc::new(Mutex::new(None)),
    });

    #[cfg(feature = "fuzzing")]
    {
        let fuzzing_config = static_prefs::pref!("fuzzing.webauthn.authenticator_config");
        if fuzzing_config != 0 {
            let is_user_verified = (fuzzing_config & 0x01) != 0;
            let is_user_consenting = (fuzzing_config & 0x02) != 0;
            let has_user_verification = (fuzzing_config & 0x04) != 0;
            let has_resident_key = (fuzzing_config & 0x08) != 0;
            let transport = nsCString::from(match (fuzzing_config & 0x10) >> 4 {
                0 => "usb",
                1 => "internal",
                _ => unreachable!(),
            });
            let protocol = nsCString::from(match (fuzzing_config & 0x60) >> 5 {
                0 => "", // reserved
                1 => "ctap1/u2f",
                2 => "ctap2",
                3 => "ctap2_1",
                _ => unreachable!(),
            });
            // If this fails it's probably because the protocol bits were zero,
            // we'll just ignore it.
            let _ = wrapper.add_virtual_authenticator(
                &protocol,
                &transport,
                has_resident_key,
                has_user_verification,
                is_user_consenting,
                is_user_verified,
            );
        }
    }

    unsafe {
        RefPtr::new(wrapper.coerce::<nsIWebAuthnTransport>()).forget(&mut *result);
    }
    NS_OK
}

#[no_mangle]
pub extern "C" fn authrs_webauthn_att_obj_constructor(
    att_obj_bytes: &ThinVec<u8>,
    anonymize: bool,
    result: *mut *const nsIWebAuthnAttObj,
) -> nsresult {
    if result.is_null() {
        return NS_ERROR_NULL_POINTER;
    }

    let mut att_obj: AttestationObject = match serde_cbor::from_slice(att_obj_bytes) {
        Ok(att_obj) => att_obj,
        Err(_) => return NS_ERROR_INVALID_ARG,
    };

    if anonymize {
        att_obj.anonymize();
    }

    let wrapper = WebAuthnAttObj::allocate(InitWebAuthnAttObj { att_obj });

    unsafe {
        RefPtr::new(wrapper.coerce::<nsIWebAuthnAttObj>()).forget(&mut *result);
    }

    NS_OK
}

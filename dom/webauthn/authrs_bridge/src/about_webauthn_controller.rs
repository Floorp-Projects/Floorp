/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::*;
use authenticator::{
    ctap2::commands::{PinUvAuthResult, StatusCode},
    errors::{CommandError, HIDError},
    BioEnrollmentCmd, CredManagementCmd, InteractiveRequest, InteractiveUpdate, PinError,
};
use serde::{Deserialize, Serialize};

pub(crate) type InteractiveManagementReceiver = Option<Sender<InteractiveRequest>>;
pub(crate) fn send_about_prompt(prompt: &BrowserPromptType) -> Result<(), nsresult> {
    let json = nsString::from(&serde_json::to_string(&prompt).unwrap_or_default());
    notify_observers(PromptTarget::AboutPage, json)
}

// A wrapper around InteractiveRequest, that leaves out the PUAT
// so that we can easily de/serialize it to/from JSON for the JS-side
// and then add our cached PUAT, if we have one.
#[derive(Debug, Serialize, Deserialize)]
pub enum RequestWrapper {
    Quit,
    ChangePIN(Pin, Pin),
    SetPIN(Pin),
    CredentialManagement(CredManagementCmd),
    BioEnrollment(BioEnrollmentCmd),
}

pub(crate) fn authrs_to_prompt<'a>(e: AuthenticatorError) -> BrowserPromptType<'a> {
    match e {
        AuthenticatorError::PinError(PinError::PinIsTooShort) => BrowserPromptType::PinIsTooShort,
        AuthenticatorError::PinError(PinError::PinNotSet) => BrowserPromptType::PinNotSet,
        AuthenticatorError::PinError(PinError::PinRequired) => BrowserPromptType::PinRequired,
        AuthenticatorError::PinError(PinError::PinIsTooLong(_)) => BrowserPromptType::PinIsTooLong,
        AuthenticatorError::PinError(PinError::InvalidPin(r)) => {
            BrowserPromptType::PinInvalid { retries: r }
        }
        AuthenticatorError::PinError(PinError::PinAuthBlocked) => BrowserPromptType::PinAuthBlocked,
        AuthenticatorError::PinError(PinError::PinBlocked) => BrowserPromptType::DeviceBlocked,
        AuthenticatorError::PinError(PinError::UvBlocked) => BrowserPromptType::UvBlocked,
        AuthenticatorError::PinError(PinError::InvalidUv(r)) => {
            BrowserPromptType::UvInvalid { retries: r }
        }
        AuthenticatorError::CancelledByUser
        | AuthenticatorError::HIDError(HIDError::Command(CommandError::StatusCode(
            StatusCode::KeepaliveCancel,
            _,
        ))) => BrowserPromptType::Cancel,
        _ => BrowserPromptType::UnknownError,
    }
}

pub(crate) fn cache_puat(
    transaction: Arc<Mutex<Option<TransactionState>>>,
    puat: Option<PinUvAuthResult>,
) {
    let mut guard = transaction.lock().unwrap();
    if let Some(transaction) = guard.as_mut() {
        transaction.puat_cache = puat;
    };
}

pub(crate) fn interactive_status_callback(
    status_rx: Receiver<StatusUpdate>,
    transaction: Arc<Mutex<Option<TransactionState>>>, /* Shared with an AuthrsTransport */
    upcoming_error: Arc<Mutex<Option<AuthenticatorError>>>,
) -> Result<(), nsresult> {
    loop {
        match status_rx.recv() {
            Ok(StatusUpdate::InteractiveManagement(InteractiveUpdate::StartManagement((
                tx,
                auth_info,
            )))) => {
                let mut guard = transaction.lock().unwrap();
                let Some(transaction) = guard.as_mut() else {
                    warn!("STATUS: received status update after end of transaction.");
                    break;
                };
                transaction.interactive_receiver.replace(tx);
                let prompt = BrowserPromptType::SelectedDevice { auth_info };
                send_about_prompt(&prompt)?;
            }
            Ok(StatusUpdate::InteractiveManagement(
                InteractiveUpdate::CredentialManagementUpdate((cfg_result, puat_res)),
            )) => {
                cache_puat(transaction.clone(), puat_res); // We don't care if we fail here. Worst-case: User has to enter PIN more often.
                let prompt = BrowserPromptType::CredentialManagementUpdate { result: cfg_result };
                send_about_prompt(&prompt)?;
                continue;
            }
            Ok(StatusUpdate::InteractiveManagement(InteractiveUpdate::BioEnrollmentUpdate((
                bio_res,
                puat_res,
            )))) => {
                cache_puat(transaction.clone(), puat_res); // We don't care if we fail here. Worst-case: User has to enter PIN more often.
                let prompt = BrowserPromptType::BioEnrollmentUpdate { result: bio_res };
                send_about_prompt(&prompt)?;
                continue;
            }
            Ok(StatusUpdate::SelectDeviceNotice) => {
                info!("STATUS: Please select a device by touching one of them.");
                let prompt = BrowserPromptType::SelectDevice;
                send_about_prompt(&prompt)?;
            }
            Ok(StatusUpdate::PinUvError(e)) => {
                let mut guard = transaction.lock().unwrap();
                let Some(transaction) = guard.as_mut() else {
                    warn!("STATUS: received status update after end of transaction.");
                    break;
                };
                let autherr = match e {
                    StatusPinUv::PinRequired(pin_sender) => {
                        transaction.pin_receiver.replace((0, pin_sender));
                        send_about_prompt(&BrowserPromptType::PinRequired)?;
                        continue;
                    }
                    StatusPinUv::InvalidPin(pin_sender, r) => {
                        transaction.pin_receiver.replace((0, pin_sender));
                        send_about_prompt(&BrowserPromptType::PinInvalid { retries: r })?;
                        continue;
                    }
                    StatusPinUv::PinIsTooShort => {
                        AuthenticatorError::PinError(PinError::PinIsTooShort)
                    }
                    StatusPinUv::PinIsTooLong(s) => {
                        AuthenticatorError::PinError(PinError::PinIsTooLong(s))
                    }
                    StatusPinUv::InvalidUv(r) => {
                        send_about_prompt(&BrowserPromptType::UvInvalid { retries: r })?;
                        continue;
                    }
                    StatusPinUv::PinAuthBlocked => {
                        AuthenticatorError::PinError(PinError::PinAuthBlocked)
                    }
                    StatusPinUv::PinBlocked => AuthenticatorError::PinError(PinError::PinBlocked),
                    StatusPinUv::PinNotSet => AuthenticatorError::PinError(PinError::PinNotSet),
                    StatusPinUv::UvBlocked => AuthenticatorError::PinError(PinError::UvBlocked),
                };
                // We will cause auth-rs to return an error, once we leave this block
                // due to us 'hanging up'. Before we do that, we will safe the actual
                // error that caused this, so our callback-function can return the true
                // error to JS, instead of "cancelled by user".
                let guard = upcoming_error.lock();
                if let Ok(mut entry) = guard {
                    entry.replace(autherr);
                } else {
                    return Err(NS_ERROR_DOM_INVALID_STATE_ERR);
                }
                warn!("STATUS: Pin Error {:?}", e);
                break;
            }

            Ok(_) => {
                // currently not handled
                continue;
            }
            Err(RecvError) => {
                info!("STATUS: end");
                break;
            }
        }
    }
    Ok(())
}

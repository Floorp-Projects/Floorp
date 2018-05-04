/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate base64;
extern crate sha2;
extern crate u2fhid;
use sha2::{Digest, Sha256};
use std::io;
use std::sync::mpsc::channel;
use u2fhid::{AuthenticatorTransports, KeyHandle, RegisterFlags, SignFlags, U2FManager};

extern crate env_logger;
extern crate log;

macro_rules! try_or {
    ($val:expr, $or:expr) => {
        match $val {
            Ok(v) => v,
            Err(e) => {
                return $or(e);
            }
        }
    };
}

fn u2f_get_key_handle_from_register_response(register_response: &Vec<u8>) -> io::Result<Vec<u8>> {
    if register_response[0] != 0x05 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "Reserved byte not set correctly",
        ));
    }

    let key_handle_len = register_response[66] as usize;
    let mut public_key = register_response.clone();
    let mut key_handle = public_key.split_off(67);
    let _attestation = key_handle.split_off(key_handle_len);

    Ok(key_handle)
}

fn main() {
    env_logger::init();

    println!("Asking a security key to register now...");
    let challenge_str = format!("{}{}",
        r#"{"challenge": "1vQ9mxionq0ngCnjD-wTsv1zUSrGRtFqG2xP09SbZ70","#,
        r#" "version": "U2F_V2", "appId": "http://demo.yubico.com"}"#);
    let mut challenge = Sha256::default();
    challenge.input(challenge_str.as_bytes());
    let chall_bytes = challenge.result().to_vec();

    let mut application = Sha256::default();
    application.input(b"http://demo.yubico.com");
    let app_bytes = application.result().to_vec();

    let manager = U2FManager::new().unwrap();
    let flags = RegisterFlags::empty();

    let (tx, rx) = channel();
    manager
        .register(
            flags,
            15_000,
            chall_bytes.clone(),
            app_bytes.clone(),
            vec![],
            move |rv| {
                tx.send(rv.unwrap()).unwrap();
            },
        )
        .unwrap();

    let register_data = try_or!(rx.recv(), |_| {
        panic!("Problem receiving, unable to continue");
    });
    println!("Register result: {}", base64::encode(&register_data));
    println!("Asking a security key to sign now, with the data from the register...");
    let credential = u2f_get_key_handle_from_register_response(&register_data).unwrap();
    let key_handle = KeyHandle {
        credential,
        transports: AuthenticatorTransports::empty(),
    };

    let flags = SignFlags::empty();
    let (tx, rx) = channel();
    manager
        .sign(
            flags,
            15_000,
            chall_bytes,
            vec![app_bytes],
            vec![key_handle],
            move |rv| {
                tx.send(rv.unwrap()).unwrap();
            },
        )
        .unwrap();

    let (_, handle_used, sign_data) = try_or!(rx.recv(), |_| {
        println!("Problem receiving");
    });
    println!("Sign result: {}", base64::encode(&sign_data));
    println!("Key handle used: {}", base64::encode(&handle_used));
    println!("Done.");
}

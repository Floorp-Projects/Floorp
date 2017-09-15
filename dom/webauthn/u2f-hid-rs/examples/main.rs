/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate base64;
extern crate crypto;
extern crate u2fhid;
use crypto::digest::Digest;
use crypto::sha2::Sha256;
use std::io;
use std::sync::mpsc::channel;
use u2fhid::U2FManager;

extern crate log;
extern crate env_logger;

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
    env_logger::init().expect("Cannot start logger");

    println!("Asking a security key to register now...");
    let challenge_str =
        format!("{}{}",
        r#"{"challenge": "1vQ9mxionq0ngCnjD-wTsv1zUSrGRtFqG2xP09SbZ70","#,
        r#" "version": "U2F_V2", "appId": "http://demo.yubico.com"}"#);
    let mut challenge = Sha256::new();
    challenge.input_str(&challenge_str);
    let mut chall_bytes: Vec<u8> = vec![0; challenge.output_bytes()];
    challenge.result(&mut chall_bytes);

    let mut application = Sha256::new();
    application.input_str("http://demo.yubico.com");
    let mut app_bytes: Vec<u8> = vec![0; application.output_bytes()];
    application.result(&mut app_bytes);

    let manager = U2FManager::new().unwrap();

    let (tx, rx) = channel();
    manager
        .register(15_000, chall_bytes.clone(), app_bytes.clone(), move |rv| {
            tx.send(rv.unwrap()).unwrap();
        })
        .unwrap();

    let register_data = rx.recv().unwrap();
    println!("Register result: {}", base64::encode(&register_data));
    println!("Asking a security key to sign now, with the data from the register...");
    let key_handle = u2f_get_key_handle_from_register_response(&register_data).unwrap();

    let (tx, rx) = channel();
    manager
        .sign(
            15_000,
            chall_bytes,
            app_bytes,
            vec![key_handle],
            move |rv| { tx.send(rv.unwrap()).unwrap(); },
        )
        .unwrap();

    let (_, sign_data) = rx.recv().unwrap();
    println!("Sign result: {}", base64::encode(&sign_data));
    println!("Done.");
}

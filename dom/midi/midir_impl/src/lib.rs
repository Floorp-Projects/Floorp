extern crate thin_vec;

use midir::{
    InitError, MidiInput, MidiInputConnection, MidiInputPort, MidiOutput, MidiOutputConnection,
    MidiOutputPort,
};
use nsstring::{nsAString, nsString};
use std::boxed::Box;
use std::ptr;
use thin_vec::ThinVec;
use uuid::Uuid;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
extern crate midir;

#[cfg(target_os = "windows")]
#[repr(C)]
#[derive(Clone, Copy)]
pub struct GeckoTimeStamp {
    gtc: u64,
    qpc: u64,

    is_null: u8,
    has_qpc: u8,
}

#[cfg(not(target_os = "windows"))]
#[repr(C)]
#[derive(Clone, Copy)]
pub struct GeckoTimeStamp {
    value: u64,
}

enum MidiConnection {
    Input(MidiInputConnection<CallbackData>),
    Output(MidiOutputConnection),
}

struct MidiConnectionWrapper {
    id: String,
    connection: MidiConnection,
}

enum MidiPort {
    Input(MidiInputPort),
    Output(MidiOutputPort),
}

struct MidiPortWrapper {
    id: String,
    name: String,
    port: MidiPort,
}

pub struct MidirWrapper {
    ports: Vec<MidiPortWrapper>,
    connections: Vec<MidiConnectionWrapper>,
}

struct CallbackData {
    nsid: nsString,
    open_timestamp: GeckoTimeStamp,
}

impl MidirWrapper {
    fn open_port(
        self: &mut MidirWrapper,
        nsid: &nsString,
        timestamp: GeckoTimeStamp,
        callback: unsafe extern "C" fn(
            id: &nsString,
            data: *const u8,
            length: usize,
            timestamp: &GeckoTimeStamp,
            micros: u64,
        ),
    ) -> Result<(), ()> {
        let id = nsid.to_string();
        let ports = &self.ports;
        let connections = &mut self.connections;
        let port = ports.iter().find(|e| e.id.eq(&id));
        if let Some(port) = port {
            match &port.port {
                MidiPort::Input(port) => {
                    let input = MidiInput::new("WebMIDI input").map_err(|_err| ())?;
                    let data = CallbackData {
                        nsid: nsid.clone(),
                        open_timestamp: timestamp,
                    };
                    let connection = input
                        .connect(
                            port,
                            "Input connection",
                            move |stamp, message, data| unsafe {
                                callback(
                                    &data.nsid,
                                    message.as_ptr(),
                                    message.len(),
                                    &data.open_timestamp,
                                    stamp,
                                );
                            },
                            data,
                        )
                        .map_err(|_err| ())?;
                    let connection_wrapper = MidiConnectionWrapper {
                        id: id.clone(),
                        connection: MidiConnection::Input(connection),
                    };
                    connections.push(connection_wrapper);
                    return Ok(());
                }
                MidiPort::Output(port) => {
                    let output = MidiOutput::new("WebMIDI output").map_err(|_err| ())?;
                    let connection = output
                        .connect(port, "Output connection")
                        .map_err(|_err| ())?;
                    let connection_wrapper = MidiConnectionWrapper {
                        connection: MidiConnection::Output(connection),
                        id: id.clone(),
                    };
                    connections.push(connection_wrapper);
                    return Ok(());
                }
            }
        }

        Err(())
    }

    fn close_port(self: &mut MidirWrapper, id: &str) {
        let connections = &mut self.connections;
        let index = connections.iter().position(|e| e.id.eq(id)).unwrap();
        let connection_wrapper = connections.remove(index);

        match connection_wrapper.connection {
            MidiConnection::Input(connection) => {
                connection.close();
            }
            MidiConnection::Output(connection) => {
                connection.close();
            }
        }
    }

    fn send(self: &mut MidirWrapper, id: &str, data: &[u8]) -> Result<(), ()> {
        let connections = &mut self.connections;
        let index = connections.iter().position(|e| e.id.eq(id)).unwrap();
        let connection_wrapper = connections.get_mut(index).unwrap();

        match &mut connection_wrapper.connection {
            MidiConnection::Output(connection) => {
                connection.send(data).map_err(|_err| ())?;
            }
            _ => {
                panic!("Sending on an input port!");
            }
        }

        Ok(())
    }
}

impl MidirWrapper {
    fn new() -> Result<MidirWrapper, InitError> {
        let input = MidiInput::new("WebMIDI input")?;
        let output = MidiOutput::new("WebMIDI output")?;
        let mut ports: Vec<MidiPortWrapper> = Vec::new();
        collect_input_ports(&input, &mut ports);
        collect_output_ports(&output, &mut ports);
        let connections: Vec<MidiConnectionWrapper> = Vec::new();
        Ok(MidirWrapper { ports, connections })
    }
}

/// Create the C++ wrapper that will be used to talk with midir.
///
/// This function will be exposed to C++
///
/// # Safety
///
/// This function deliberately leaks the wrapper because ownership is
/// transfered to the C++ code. Use [midir_impl_shutdown()] to free it.
#[no_mangle]
pub unsafe extern "C" fn midir_impl_init() -> *mut MidirWrapper {
    if let Ok(midir_impl) = MidirWrapper::new() {
        let midir_box = Box::new(midir_impl);
        // Leak the object as it will be owned by the C++ code from now on
        Box::leak(midir_box) as *mut _
    } else {
        ptr::null_mut()
    }
}

/// Shutdown midir and free the C++ wrapper.
///
/// This function will be exposed to C++
///
/// # Safety
///
/// `wrapper` must be the pointer returned by [midir_impl_init()]. After this
/// has been called the wrapper object will be destoyed and cannot be accessed
/// anymore.
#[no_mangle]
pub unsafe extern "C" fn midir_impl_shutdown(wrapper: *mut MidirWrapper) {
    // The MidirImpl object will be automatically destroyed when the contents
    // of this box are automatically dropped at the end of the function
    let _midir_box = Box::from_raw(wrapper);
}

/// Enumerate the available MIDI ports.
///
/// This function will be exposed to C++
///
/// # Safety
///
/// `wrapper` must be the pointer returned by [midir_impl_init()].
#[no_mangle]
pub unsafe extern "C" fn midir_impl_enum_ports(
    wrapper: *mut MidirWrapper,
    callback: unsafe extern "C" fn(id: &nsString, name: &nsString, input: bool),
) {
    iterate_ports(&(*wrapper).ports, callback);
}

/// Open a MIDI port.
///
/// This function will be exposed to C++
///
/// # Safety
///
/// `wrapper` must be the pointer returned by [midir_impl_init()].
#[no_mangle]
pub unsafe extern "C" fn midir_impl_open_port(
    wrapper: *mut MidirWrapper,
    nsid: *mut nsString,
    timestamp: *mut GeckoTimeStamp,
    callback: unsafe extern "C" fn(
        id: &nsString,
        data: *const u8,
        length: usize,
        timestamp: &GeckoTimeStamp,
        micros: u64,
    ),
) -> bool {
    (*wrapper)
        .open_port(nsid.as_ref().unwrap(), *timestamp, callback)
        .is_ok()
}

/// Close a MIDI port.
///
/// This function will be exposed to C++
///
/// # Safety
///
/// `wrapper` must be the pointer returned by [midir_impl_init()].
#[no_mangle]
pub unsafe extern "C" fn midir_impl_close_port(wrapper: *mut MidirWrapper, id: *mut nsString) {
    (*wrapper).close_port(&(*id).to_string());
}

/// Send a message over a MIDI output port.
///
/// This function will be exposed to C++
///
/// # Safety
///
/// `wrapper` must be the pointer returned by [midir_impl_init()].
#[no_mangle]
pub unsafe extern "C" fn midir_impl_send(
    wrapper: *mut MidirWrapper,
    id: *const nsAString,
    data: *const ThinVec<u8>,
) -> bool {
    (*wrapper)
        .send(&(*id).to_string(), (*data).as_slice())
        .is_ok()
}

fn collect_input_ports(input: &MidiInput, wrappers: &mut Vec<MidiPortWrapper>) {
    let ports = input.ports();
    for port in ports {
        let id = Uuid::new_v4()
            .to_hyphenated()
            .encode_lower(&mut Uuid::encode_buffer())
            .to_owned();
        let name = input
            .port_name(&port)
            .unwrap_or_else(|_| "unknown input port".to_string());
        let port = MidiPortWrapper {
            id,
            name,
            port: MidiPort::Input(port),
        };
        wrappers.push(port);
    }
}

fn collect_output_ports(output: &MidiOutput, wrappers: &mut Vec<MidiPortWrapper>) {
    let ports = output.ports();
    for port in ports {
        let id = Uuid::new_v4()
            .to_hyphenated()
            .encode_lower(&mut Uuid::encode_buffer())
            .to_owned();
        let name = output
            .port_name(&port)
            .unwrap_or_else(|_| "unknown input port".to_string());
        let port = MidiPortWrapper {
            id,
            name,
            port: MidiPort::Output(port),
        };
        wrappers.push(port);
    }
}

unsafe fn iterate_ports(
    ports: &[MidiPortWrapper],
    callback: unsafe extern "C" fn(id: &nsString, name: &nsString, input: bool),
) {
    for port in ports {
        let id = nsString::from(&port.id);
        let name = nsString::from(&port.name);
        let input = match port.port {
            MidiPort::Input(_) => true,
            MidiPort::Output(_) => false,
        };
        callback(&id, &name, input);
    }
}

extern crate thin_vec;

use midir::{
    InitError, MidiInput, MidiInputConnection, MidiInputPort, MidiOutput, MidiOutputConnection,
    MidiOutputPort,
};
use nsstring::{nsAString, nsString};
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
    open_count: u32,
}

impl MidiPortWrapper {
    fn input(self: &MidiPortWrapper) -> bool {
        match self.port {
            MidiPort::Input(_) => true,
            MidiPort::Output(_) => false,
        }
    }
}

pub struct MidirWrapper {
    ports: Vec<MidiPortWrapper>,
    connections: Vec<MidiConnectionWrapper>,
}

struct CallbackData {
    nsid: nsString,
    open_timestamp: GeckoTimeStamp,
}

type AddCallback = unsafe extern "C" fn(id: &nsString, name: &nsString, input: bool);
type RemoveCallback = AddCallback;

impl MidirWrapper {
    fn refresh(
        self: &mut MidirWrapper,
        add_callback: AddCallback,
        remove_callback: Option<RemoveCallback>,
    ) {
        if let Ok(ports) = collect_ports() {
            if let Some(remove_callback) = remove_callback {
                self.remove_missing_ports(&ports, remove_callback);
            }

            self.add_new_ports(ports, add_callback);
        }
    }

    fn remove_missing_ports(
        self: &mut MidirWrapper,
        ports: &Vec<MidiPortWrapper>,
        remove_callback: RemoveCallback,
    ) {
        let old_ports = &mut self.ports;
        let mut i = 0;
        while i < old_ports.len() {
            if !ports
                .iter()
                .any(|p| p.name == old_ports[i].name && p.input() == old_ports[i].input())
            {
                let port = old_ports.remove(i);
                let id = nsString::from(&port.id);
                let name = nsString::from(&port.name);
                unsafe { remove_callback(&id, &name, port.input()) };
            } else {
                i += 1;
            }
        }
    }

    fn add_new_ports(
        self: &mut MidirWrapper,
        ports: Vec<MidiPortWrapper>,
        add_callback: AddCallback,
    ) {
        for port in ports {
            if !self.is_port_present(&port) && !Self::is_microsoft_synth_output(&port) {
                let id = nsString::from(&port.id);
                let name = nsString::from(&port.name);
                unsafe { add_callback(&id, &name, port.input()) };
                self.ports.push(port);
            }
        }
    }

    fn is_port_present(self: &MidirWrapper, port: &MidiPortWrapper) -> bool {
        self.ports
            .iter()
            .any(|p| p.name == port.name && p.input() == port.input())
    }

    // We explicitly disable Microsoft's soft synthesizer, see bug 1798097
    fn is_microsoft_synth_output(port: &MidiPortWrapper) -> bool {
        (port.input() == false) && (port.name == "Microsoft GS Wavetable Synth")
    }

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
        let connections = &mut self.connections;
        let port = self.ports.iter_mut().find(|e| e.id.eq(&id));
        if let Some(port) = port {
            if port.open_count == 0 {
                let connection = match &port.port {
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
                        MidiConnectionWrapper {
                            id: id.clone(),
                            connection: MidiConnection::Input(connection),
                        }
                    }
                    MidiPort::Output(port) => {
                        let output = MidiOutput::new("WebMIDI output").map_err(|_err| ())?;
                        let connection = output
                            .connect(port, "Output connection")
                            .map_err(|_err| ())?;
                        MidiConnectionWrapper {
                            connection: MidiConnection::Output(connection),
                            id: id.clone(),
                        }
                    }
                };

                connections.push(connection);
            }

            port.open_count += 1;
            return Ok(());
        }

        Err(())
    }

    fn close_port(self: &mut MidirWrapper, id: &str) {
        let port = self.ports.iter_mut().find(|e| e.id.eq(&id)).unwrap();
        port.open_count -= 1;

        if port.open_count > 0 {
            return;
        }

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
        let index = connections.iter().position(|e| e.id.eq(id)).ok_or(())?;
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

fn collect_ports() -> Result<Vec<MidiPortWrapper>, InitError> {
    let input = MidiInput::new("WebMIDI input")?;
    let output = MidiOutput::new("WebMIDI output")?;
    let mut ports = Vec::<MidiPortWrapper>::new();
    collect_input_ports(&input, &mut ports);
    collect_output_ports(&output, &mut ports);
    Ok(ports)
}

impl MidirWrapper {
    fn new() -> Result<MidirWrapper, InitError> {
        let ports = Vec::new();
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
pub unsafe extern "C" fn midir_impl_init(callback: AddCallback) -> *mut MidirWrapper {
    if let Ok(mut midir_impl) = MidirWrapper::new() {
        midir_impl.refresh(callback, None);

        // Gecko invokes this initialization on a separate thread from all the
        // other operations, so make it clear to Rust this needs to be Send.
        fn assert_send<T: Send>(_: &T) {}
        assert_send(&midir_impl);

        let midir_box = Box::new(midir_impl);
        // Leak the object as it will be owned by the C++ code from now on
        Box::leak(midir_box) as *mut _
    } else {
        ptr::null_mut()
    }
}

/// Refresh the list of ports.
///
/// This function will be exposed to C++
///
/// # Safety
///
/// `wrapper` must be the pointer returned by [midir_impl_init()].
#[no_mangle]
pub unsafe extern "C" fn midir_impl_refresh(
    wrapper: *mut MidirWrapper,
    add_callback: AddCallback,
    remove_callback: RemoveCallback,
) {
    (*wrapper).refresh(add_callback, Some(remove_callback))
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
            .as_hyphenated()
            .encode_lower(&mut Uuid::encode_buffer())
            .to_owned();
        let name = input
            .port_name(&port)
            .unwrap_or_else(|_| "unknown input port".to_string());
        let port = MidiPortWrapper {
            id,
            name,
            port: MidiPort::Input(port),
            open_count: 0,
        };
        wrappers.push(port);
    }
}

fn collect_output_ports(output: &MidiOutput, wrappers: &mut Vec<MidiPortWrapper>) {
    let ports = output.ports();
    for port in ports {
        let id = Uuid::new_v4()
            .as_hyphenated()
            .encode_lower(&mut Uuid::encode_buffer())
            .to_owned();
        let name = output
            .port_name(&port)
            .unwrap_or_else(|_| "unknown input port".to_string());
        let port = MidiPortWrapper {
            id,
            name,
            port: MidiPort::Output(port),
            open_count: 0,
        };
        wrappers.push(port);
    }
}

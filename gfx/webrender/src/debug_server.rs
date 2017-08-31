/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ApiMsg, DebugCommand};
use api::channel::MsgSender;
use std::sync::mpsc::{channel, Receiver};
use std::thread;
use std::sync::mpsc::Sender;

use ws;

// Messages that are sent from the render backend to the renderer
// debug command queue. These are sent in a separate queue so
// that none of these types are exposed to the RenderApi interfaces.
// We can't use select!() as it's not stable...
pub enum DebugMsg {
    FetchPasses(ws::Sender),
}

// Represents a connection to a client.
struct Server {
    ws: ws::Sender,
    debug_tx: Sender<DebugMsg>,
    api_tx: MsgSender<ApiMsg>,
}

impl ws::Handler for Server {
    fn on_message(&mut self, msg: ws::Message) -> ws::Result<()> {
        match msg {
            ws::Message::Text(string) => {
                let cmd = match string.as_str() {
                    "enable_profiler" => {
                        DebugCommand::EnableProfiler(true)
                    }
                    "disable_profiler" => {
                        DebugCommand::EnableProfiler(false)
                    }
                    "enable_texture_cache_debug" => {
                        DebugCommand::EnableTextureCacheDebug(true)
                    }
                    "disable_texture_cache_debug" => {
                        DebugCommand::EnableTextureCacheDebug(false)
                    }
                    "enable_render_target_debug" => {
                        DebugCommand::EnableRenderTargetDebug(true)
                    }
                    "disable_render_target_debug" => {
                        DebugCommand::EnableRenderTargetDebug(false)
                    }
                    "fetch_passes" => {
                        let msg = DebugMsg::FetchPasses(self.ws.clone());
                        self.debug_tx.send(msg).unwrap();
                        DebugCommand::Flush
                    }
                    msg => {
                        println!("unknown msg {}", msg);
                        return Ok(());
                    }
                };

                let msg = ApiMsg::DebugCommand(cmd);
                self.api_tx.send(msg).unwrap();
            }
            ws::Message::Binary(..) => {}
        }

        Ok(())
    }
}

// Spawn a thread for a given renderer, and wait for
// client connections.
pub struct DebugServer {
    join_handle: Option<thread::JoinHandle<()>>,
    broadcaster: ws::Sender,
    pub debug_rx: Receiver<DebugMsg>,
}

impl DebugServer {
    pub fn new(api_tx: MsgSender<ApiMsg>) -> DebugServer {
        let (debug_tx, debug_rx) = channel();

        let socket = ws::Builder::new().build(move |out| {
            Server {
                ws: out,
                debug_tx: debug_tx.clone(),
                api_tx: api_tx.clone(),
            }
        }).unwrap();

        let broadcaster = socket.broadcaster();

        let join_handle = Some(thread::spawn(move || {
            if let Err(..) = socket.listen("127.0.0.1:3583") {
                println!("ERROR: Unable to bind debugger websocket (port may be in use).");
            }
        }));

        DebugServer {
            join_handle,
            broadcaster,
            debug_rx,
        }
    }
}

impl Drop for DebugServer {
    fn drop(&mut self) {
        self.broadcaster.shutdown().ok();
        self.join_handle.take().unwrap().join().ok();
    }
}

// A serializable list of debug information about passes
// that can be sent to the client.

#[derive(Serialize)]
pub enum BatchKind {
    Clip,
    Cache,
    Opaque,
    Alpha,
}

#[derive(Serialize)]
pub struct PassList {
    kind: &'static str,
    passes: Vec<Pass>,
}

impl PassList {
    pub fn new() -> PassList {
        PassList {
            kind: "passes",
            passes: Vec::new(),
        }
    }

    pub fn add(&mut self, pass: Pass) {
        self.passes.push(pass);
    }
}

#[derive(Serialize)]
pub struct Pass {
    targets: Vec<Target>,
}

impl Pass {
    pub fn new() -> Pass {
        Pass {
            targets: Vec::new(),
        }
    }

    pub fn add(&mut self, target: Target) {
        self.targets.push(target);
    }
}

#[derive(Serialize)]
pub struct Target {
    kind: &'static str,
    batches: Vec<Batch>,
}

impl Target {
    pub fn new(kind: &'static str) -> Target {
        Target {
            kind,
            batches: Vec::new(),
        }
    }

    pub fn add(&mut self, kind: BatchKind, description: &str, count: usize) {
        if count > 0 {
            self.batches.push(Batch {
                kind,
                description: description.to_owned(),
                count,
            });
        }
    }
}

#[derive(Serialize)]
struct Batch {
    kind: BatchKind,
    description: String,
    count: usize,
}

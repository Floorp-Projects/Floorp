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
    FetchBatches(ws::Sender),
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
                    "fetch_batches" => {
                        let msg = DebugMsg::FetchBatches(self.ws.clone());
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
            socket.listen("127.0.0.1:3583").unwrap();
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
        self.broadcaster.shutdown().unwrap();
        self.join_handle.take().unwrap().join().unwrap();
    }
}

// A serializable list of debug information about batches
// that can be sent to the client.
#[derive(Serialize)]
pub struct BatchInfo {
    kind: &'static str,
    count: usize,
}

#[derive(Serialize)]
pub struct BatchList {
    kind: &'static str,
    batches: Vec<BatchInfo>,
}

impl BatchList {
    pub fn new() -> BatchList {
        BatchList {
            kind: "batches",
            batches: Vec::new(),
        }
    }

    pub fn push(&mut self, kind: &'static str, count: usize) {
        if count > 0 {
            self.batches.push(BatchInfo {
                kind,
                count,
            });
        }
    }
}

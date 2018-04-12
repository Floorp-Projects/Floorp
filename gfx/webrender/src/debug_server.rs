/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ApiMsg, DebugCommand};
use api::channel::MsgSender;
use print_tree::PrintTreePrinter;
use std::sync::mpsc::{channel, Receiver};
use std::sync::mpsc::Sender;
use std::thread;
use ws;
use base64::encode;
use image_loader;

// Messages that are sent from the render backend to the renderer
// debug command queue. These are sent in a separate queue so
// that none of these types are exposed to the RenderApi interfaces.
// We can't use select!() as it's not stable...
enum DebugMsg {
    AddSender(ws::Sender),
    RemoveSender(ws::util::Token),
}

// Represents a connection to a client.
struct Server {
    ws: ws::Sender,
    debug_tx: Sender<DebugMsg>,
    api_tx: MsgSender<ApiMsg>,
}

impl ws::Handler for Server {
    fn on_open(&mut self, _: ws::Handshake) -> ws::Result<()> {
        self.debug_tx
            .send(DebugMsg::AddSender(self.ws.clone()))
            .ok();

        Ok(())
    }

    fn on_close(&mut self, _: ws::CloseCode, _: &str) {
        self.debug_tx
            .send(DebugMsg::RemoveSender(self.ws.token()))
            .ok();
    }

    fn on_message(&mut self, msg: ws::Message) -> ws::Result<()> {
        match msg {
            ws::Message::Text(string) => {
                let cmd = match string.as_str() {
                    "enable_profiler" => DebugCommand::EnableProfiler(true),
                    "disable_profiler" => DebugCommand::EnableProfiler(false),
                    "enable_texture_cache_debug" => DebugCommand::EnableTextureCacheDebug(true),
                    "disable_texture_cache_debug" => DebugCommand::EnableTextureCacheDebug(false),
                    "enable_render_target_debug" => DebugCommand::EnableRenderTargetDebug(true),
                    "disable_render_target_debug" => DebugCommand::EnableRenderTargetDebug(false),
                    "enable_gpu_time_queries" => DebugCommand::EnableGpuTimeQueries(true),
                    "disable_gpu_time_queries" => DebugCommand::EnableGpuTimeQueries(false),
                    "enable_gpu_sample_queries" => DebugCommand::EnableGpuSampleQueries(true),
                    "disable_gpu_sample_queries" => DebugCommand::EnableGpuSampleQueries(false),
                    "fetch_passes" => DebugCommand::FetchPasses,
                    "fetch_screenshot" => DebugCommand::FetchScreenshot,
                    "fetch_documents" => DebugCommand::FetchDocuments,
                    "fetch_clip_scroll_tree" => DebugCommand::FetchClipScrollTree,
                    "fetch_render_tasks" => DebugCommand::FetchRenderTasks,
                    msg => {
                        error!("unknown msg {}", msg);
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
    debug_rx: Receiver<DebugMsg>,
    senders: Vec<ws::Sender>,
}

impl DebugServer {
    pub fn new(api_tx: MsgSender<ApiMsg>) -> DebugServer {
        let (debug_tx, debug_rx) = channel();

        let socket = ws::Builder::new()
            .build(move |out| {
                Server {
                    ws: out,
                    debug_tx: debug_tx.clone(),
                    api_tx: api_tx.clone(),
                }
            })
            .unwrap();

        let broadcaster = socket.broadcaster();

        let join_handle = Some(thread::spawn(move || {
            let address = "127.0.0.1:3583";
            debug!("WebRender debug server started: {}", address);
            if let Err(..) = socket.listen(address) {
                error!("ERROR: Unable to bind debugger websocket (port may be in use).");
            }
        }));

        DebugServer {
            join_handle,
            broadcaster,
            debug_rx,
            senders: Vec::new(),
        }
    }

    pub fn send(&mut self, message: String) {
        // Add any new connections that have been queued.
        while let Ok(msg) = self.debug_rx.try_recv() {
            match msg {
                DebugMsg::AddSender(sender) => {
                    self.senders.push(sender);
                }
                DebugMsg::RemoveSender(token) => {
                    self.senders.retain(|sender| sender.token() != token);
                }
            }
        }

        // Broadcast the message to all senders. Keep
        // track of the ones that failed, so they can
        // be removed from the active sender list.
        let mut disconnected_senders = Vec::new();

        for (i, sender) in self.senders.iter().enumerate() {
            if let Err(..) = sender.send(message.clone()) {
                disconnected_senders.push(i);
            }
        }

        // Remove the broken senders from the list
        // for next broadcast. Remove in reverse
        // order so the indices are valid for the
        // entire loop.
        for i in disconnected_senders.iter().rev() {
            self.senders.remove(*i);
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
    pub targets: Vec<Target>,
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

#[derive(Serialize)]
pub struct TreeNode {
    description: String,
    children: Vec<TreeNode>,
}

impl TreeNode {
    pub fn new(description: &str) -> TreeNode {
        TreeNode {
            description: description.to_owned(),
            children: Vec::new(),
        }
    }

    pub fn add_child(&mut self, child: TreeNode) {
        self.children.push(child);
    }

    pub fn add_item(&mut self, description: &str) {
        self.children.push(TreeNode::new(description));
    }
}

#[derive(Serialize)]
pub struct DocumentList {
    kind: &'static str,
    root: TreeNode,
}

impl DocumentList {
    pub fn new() -> Self {
        DocumentList {
            kind: "documents",
            root: TreeNode::new("root"),
        }
    }

    pub fn add(&mut self, item: TreeNode) {
        self.root.add_child(item);
    }
}

#[derive(Serialize)]
pub struct Screenshot {
    kind: &'static str,
    data: String
}

impl Screenshot {
    pub fn new(width: u32, height: u32, data: Vec<u8>) -> Self {
        let mut output = Vec::with_capacity((width * height) as usize);
        {
            let encoder = image_loader::png::PNGEncoder::new(&mut output);
            encoder.encode(&data, width, height, image_loader::ColorType::RGBA(8)).unwrap();
        }

        let data = encode(&output);
        Screenshot {
            kind: "screenshot",
            data
        }
    }
}

// A serializable list of debug information about clip-scroll trees
// that can be sent to the client

#[derive(Serialize)]
pub struct ClipScrollTreeList {
    kind: &'static str,
    root: TreeNode,
}

impl ClipScrollTreeList {
    pub fn new() -> Self {
        ClipScrollTreeList {
            kind: "clip_scroll_tree",
            root: TreeNode::new("root"),
        }
    }

    pub fn add(&mut self, item: TreeNode) {
        self.root.add_child(item);
    }
}

#[derive(Serialize)]
pub struct RenderTaskList {
    kind: &'static str,
    root: TreeNode,
}

impl RenderTaskList {
    pub fn new() -> Self {
        RenderTaskList {
            kind: "render_tasks",
            root: TreeNode::new("root"),
        }
    }

    pub fn add(&mut self, item: TreeNode) {
        self.root.add_child(item);
    }
}

// A TreeNode-based PrintTreePrinter to serialize pretty-printed
// trees as json
pub struct TreeNodeBuilder {
    levels: Vec<TreeNode>,
}

impl TreeNodeBuilder {
    pub fn new(root: TreeNode) -> TreeNodeBuilder {
        TreeNodeBuilder { levels: vec![root] }
    }

    fn current_level_mut(&mut self) -> &mut TreeNode {
        assert!(!self.levels.is_empty());
        self.levels.last_mut().unwrap()
    }

    pub fn build(mut self) -> TreeNode {
        assert!(self.levels.len() == 1);
        self.levels.pop().unwrap()
    }
}

impl PrintTreePrinter for TreeNodeBuilder {
    fn new_level(&mut self, title: String) {
        let level = TreeNode::new(&title);
        self.levels.push(level);
    }

    fn end_level(&mut self) {
        assert!(!self.levels.is_empty());
        let last_level = self.levels.pop().unwrap();
        self.current_level_mut().add_child(last_level);
    }

    fn add_item(&mut self, text: String) {
        self.current_level_mut().add_item(&text);
    }
}

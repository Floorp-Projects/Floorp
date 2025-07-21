import type { Plugin, ViteDevServer } from "rolldown-vite";
import { type BirpcReturn, createBirpc } from "birpc";
import type {
  ClientFunctions,
  MiddlewareFunctions,
  PoolFunctions,
  RunnerUID,
  ServerFunctions,
} from "./types/rpc.d.ts";
// @deno-types="npm:@types/ws"
import { type default as WebSocket, WebSocketServer } from "npm:ws";

export default function NoranekoTestPlugin() {
  let rpc: BirpcReturn<ClientFunctions, ServerFunctions> | null = null;
  let wssPool: WebSocketServer | null = null;
  let wsPool: WebSocket | null = null;
  let rpcPool: BirpcReturn<PoolFunctions, MiddlewareFunctions> | null = null;
  let viteDevServer: ViteDevServer | null = null;

  const middleware = new (class implements MiddlewareFunctions {
    async registerNoraRunner(): Promise<RunnerUID> {
      const uid = await rpc!.registerNoraRunner();
      viteDevServer!.ws.on(`vitest-noraneko:message:${uid}`, (data, client) => {
        rpcPool!.forwardToPool(uid, data);
      });
      console.log("uid", uid);
      return uid;
    }
    forwardToRunner(uid: RunnerUID, data: any): void {
      viteDevServer!.ws.send(`vitest-noraneko:message:${uid}`, data);
    }
  })();

  return {
    name: "vitest-noraneko",
    configureServer(server) {
      viteDevServer = server;
      wssPool = new WebSocketServer({ port: 5201 });
      wssPool.on("connection", (ws) => {
        wsPool = ws;
        rpcPool = createBirpc<PoolFunctions, MiddlewareFunctions>(middleware, {
          post: (data) => wsPool!.send(data),
          on: (callback) => wsPool!.on("message", callback),
          // these are required when using WebSocket
          serialize: (v) => JSON.stringify(v),
          deserialize: (v) => JSON.parse(v),
        });
      });
      server.ws.on("vitest-noraneko:init", (data, client) => {
        console.log("init", data);
        rpc = createBirpc<ClientFunctions, ServerFunctions>(
          {},
          {
            post: (data) => client.send("vitest-noraneko:message", data),
            on: (callback) => client.on("vitest-noraneko:message", callback),
            // these are required when using WebSocket
            serialize: (v) => JSON.stringify(v),
            deserialize: (v) => JSON.parse(v),
          },
        );
      });
    },
  } satisfies Plugin;
}

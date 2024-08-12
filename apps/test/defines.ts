import { initTRPC } from "@trpc/server";
import type { CreateHTTPContextOptions } from "@trpc/server/adapters/standalone";

import type { CreateWSSContextFnOptions } from "@trpc/server/adapters/ws";

import { observable } from "@trpc/server/observable";

import { string, z } from "zod";
import { EventEmitter } from "node:events";
import fs from "node:fs/promises";

// This is how you initialize a context for the server
export function createContext(
  opts: CreateHTTPContextOptions | CreateWSSContextFnOptions,
) {
  return {};
}
type Context = Awaited<ReturnType<typeof createContext>>;

const t = initTRPC.context<Context>().create();

const publicProcedure = t.procedure;
const router = t.router;

let num = 0;

export const ee = new EventEmitter();

const postRouter = router({
  initCompleted: publicProcedure.mutation(async ({ input }) => {
    import("./tests/screenshot");
  }),
  onTestCommand: publicProcedure.subscription(() => {
    return observable<{ testName: string }>((emit) => {
      const onTestCommand = (data: { testName: string }) => {
        // emits a number every second
        emit.next({ testName: data.testName });
      };

      ee.on("testCommand", onTestCommand);

      return () => {
        ee.off("testCommand", onTestCommand);
      };
    });
  }),
  testStatus: publicProcedure
    .input(
      z.object({
        testName: z.string(),
        status: z.enum(["pending", "failure", "completed"]),
        data: z.string(),
      }),
    )
    .mutation(async ({ input }) => {
      ee.emit("testStatus", input);
    }),
  save: publicProcedure
    .input(
      z.object({
        filename: z.string(),
        image: z.string(),
      }),
    )
    .output(z.object({ status: z.boolean() }))
    .mutation(async ({ input }) => {
      //console.log(input.image);
      ee.emit("save", input);
      console.log(input.filename);
      await fs.writeFile(
        `${input.filename}.png`,
        input.image.replace(/^data:image\/png;base64,/, ""),
        "base64",
      );
      num += 1;
      return { status: true };
    }),
});

// Merge routers together
export const appRouter = router({
  post: postRouter,
});

export type AppRouter = typeof appRouter;

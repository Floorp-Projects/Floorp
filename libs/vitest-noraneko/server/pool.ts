import { createMethodsRPC, ProcessPool, TestSpecification } from "vitest/node";

import {
  RunnerRPC,
  RunnerTaskResultPack,
  RunnerTestCase,
  RunnerTestFile,
  RuntimeRPC,
} from "vitest";
import { generateFileHash, getTasks } from "@vitest/runner/utils";
import { relative } from "pathe";
import WebSocket from "isomorphic-ws";
import { BirpcReturn, createBirpc } from "birpc";
import { VitestRunner } from "@vitest/runner";
import { NoraRunner, RunnerUID } from "../types/rpc.d.ts";

function initWS() {
  const ws = new WebSocket("ws://localhost:5201");
  let r = null;
  const promise = new Promise<WebSocket>((resolve) => {
    r = resolve;
  });
  ws.on("open", () => {
    console.log("open");
    r(ws);
  });
  return promise;
}

class NoranekoTestPool implements ProcessPool, PoolFunctions {
  name: string = "vitest-noraneko-pool";

  forwarder = new Map<RunnerUID, (data: any) => void>();
  forwardToPool(uid: RunnerUID, data: any): void {
    this.forwarder.get(uid)!(data);
  }

  async runTests(files: TestSpecification[], invalidates?: string[]) {
    const ws = await initWS();
    console.log("initss");
    for (const spec of files) {
      const ctx = spec.project.vitest;
      const project = spec.project;
      // fullpath of test file
      // const path = spec.moduleId;
      console.log("before uid");
      let uid;
      try {
        uid = await wsRPC.registerNoraRunner();
      } catch (e) {
        console.trace(e);
      }
      console.log("init norarunner");
      const rpc = createBirpc<NoraRunner, {}>(this, {
        post: (data) => wsRPC.forwardToRunner(uid, data),
        on: (callback) => this.forwarder.set(uid, callback),
        // these are required when using WebSocket
        serialize: (v) => JSON.stringify(v),
        deserialize: (v) => JSON.parse(v),
      });

      // clear for when rerun
      ctx.state.clearFiles(project);
      const path = relative(project.config.root, spec.moduleId);

      // Register Test Files
      {
        const testFile: RunnerTestFile = {
          id: generateFileHash(path, project.config.name),
          name: path,
          mode: "run",
          meta: { typecheck: true },
          projectName: project.name,
          filepath: path,
          type: "suite",
          tasks: [],
          result: {
            state: "pass",
          },
          file: null!,
        };
        testFile.file = testFile;

        const taskTest: RunnerTestCase = {
          type: "test",
          name: "custom test",
          id: "custom-test",
          context: {} as any,
          suite: testFile,
          mode: "run",
          meta: {},
          result: {
            state: "pass",
          },
          file: testFile,
        };

        testFile.tasks.push(taskTest);

        await rpc!.runTest(testFile.filepath);
        console.log("run test");
        // rpc!.onCollected([testFile]);
        ctx.state.collectFiles(project, [testFile]);
        // rpc!.onTaskUpdate(
        //   getTasks(testFile).map((task) => [
        //     task.id,
        //     task.result,
        //     task.meta,
        //   ]),
        //   [],
        // );
        ctx.state.updateTasks(
          getTasks(testFile).map((task) => [task.id, task.result, task.meta]),
        );
        console.log("end");
      }
    }
  }
  collectTests(files: TestSpecification[], invalidates?: string[]) {
    for (const file of files) {
      console.log(file.moduleId);
    }
  }
  async close(): Promise<void> {}
}

export default () => new NoranekoTestPool();

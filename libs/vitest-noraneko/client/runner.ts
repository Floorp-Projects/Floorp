import type { SerializedConfig } from "vitest";
import type { VitestRunner } from "vitest/suite";
import { VitestRunnerImportSource } from "@vitest/runner";
import { NoraRunner } from "../types/rpc.d.ts";
class NoranekoRunner implements NoraRunner {
  async runTest(filepath: string): Promise<void> {
    console.log(`running test: ${filepath}`);
  }
}

export default NoranekoRunner;

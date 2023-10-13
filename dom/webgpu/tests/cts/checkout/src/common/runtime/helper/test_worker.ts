import { LogMessageWithStack } from '../../internal/logging/log_message.js';
import { TransferredTestCaseResult, LiveTestCaseResult } from '../../internal/logging/result.js';
import { TestCaseRecorder } from '../../internal/logging/test_case_recorder.js';
import { TestQueryWithExpectation } from '../../internal/query/query.js';

import { CTSOptions, kDefaultCTSOptions } from './options.js';

export class TestWorker {
  private readonly ctsOptions: CTSOptions;
  private readonly worker: Worker;
  private readonly resolvers = new Map<string, (result: LiveTestCaseResult) => void>();

  constructor(ctsOptions?: CTSOptions) {
    this.ctsOptions = { ...(ctsOptions || kDefaultCTSOptions), ...{ worker: true } };
    const selfPath = import.meta.url;
    const selfPathDir = selfPath.substring(0, selfPath.lastIndexOf('/'));
    const workerPath = selfPathDir + '/test_worker-worker.js';
    this.worker = new Worker(workerPath, { type: 'module' });
    this.worker.onmessage = ev => {
      const query: string = ev.data.query;
      const result: TransferredTestCaseResult = ev.data.result;
      if (result.logs) {
        for (const l of result.logs) {
          Object.setPrototypeOf(l, LogMessageWithStack.prototype);
        }
      }
      this.resolvers.get(query)!(result as LiveTestCaseResult);

      // MAINTENANCE_TODO(kainino0x): update the Logger with this result (or don't have a logger and
      // update the entire results JSON somehow at some point).
    };
  }

  async run(
    rec: TestCaseRecorder,
    query: string,
    expectations: TestQueryWithExpectation[] = []
  ): Promise<void> {
    this.worker.postMessage({
      query,
      expectations,
      ctsOptions: this.ctsOptions,
    });
    const workerResult = await new Promise<LiveTestCaseResult>(resolve => {
      this.resolvers.set(query, resolve);
    });
    rec.injectResult(workerResult);
  }
}

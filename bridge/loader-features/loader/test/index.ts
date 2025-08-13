// src/core/glue/loader-features/loader/test/index.ts
/**
 * Firefox Chrome環境用テストRunner
 * Vitestの指示を受けてテストを実行し、結果を返却する
 */

interface TestResult {
  passed: boolean;
  name: string;
  error?: string;
  duration: number;
}

interface TestSuite {
  name: string;
  tests: TestResult[];
  passed: boolean;
  duration: number;
}

class FirefoxChromeTestRunner {
  private testSuites: TestSuite[] = [];
  private currentSuite: TestSuite | null = null;
  private vitestConnection: WebSocket | null = null;

  async initialize() {
    console.log("[noraneko-test] Initializing Firefox Chrome Test Runner...");

    // Vitestとの通信チャネル確立
    await this.connectToVitest();

    // Firefox固有API初期化確認
    this.validateFirefoxAPIs();

    // グローバルテスト関数を登録
    this.setupTestGlobals();

    console.log("[noraneko-test] Test Runner initialized successfully");
  }

  private async connectToVitest() {
    return new Promise<void>((resolve, reject) => {
      try {
        // VitestのHMRポートに接続（通常は5181+1）
        this.vitestConnection = new WebSocket(`ws://localhost:5182/`);

        this.vitestConnection.onopen = () => {
          console.log("[noraneko-test] Connected to Vitest");
          resolve();
        };

        this.vitestConnection.onmessage = (event) => {
          this.handleVitestMessage(JSON.parse(event.data));
        };

        this.vitestConnection.onerror = (error) => {
          console.error("[noraneko-test] Vitest connection error:", error);
          reject(error);
        };
      } catch (error) {
        // WebSocket接続が失敗した場合はHTTP polling方式にフォールバック
        console.warn("[noraneko-test] WebSocket failed, using HTTP polling");
        this.setupHttpPolling();
        resolve();
      }
    });
  }

  private setupHttpPolling() {
    // HTTP polling でVitestと通信
    setInterval(async () => {
      try {
        const response = await fetch(
          "http://localhost:5181/__vitest_runner_poll",
        );
        if (response.ok) {
          const data = await response.json();
          this.handleVitestMessage(data);
        }
      } catch (error) {
        // ポーリングエラーは無視（Vitestが起動していない可能性）
      }
    }, 1000);
  }

  private validateFirefoxAPIs() {
    const requiredAPIs = ["ChromeUtils", "Services", "Cu", "Components"];
    const missingAPIs = requiredAPIs.filter(
      (api) => typeof globalThis[api] === "undefined",
    );

    if (missingAPIs.length > 0) {
      throw new Error(`Missing Firefox APIs: ${missingAPIs.join(", ")}`);
    }

    console.log("[noraneko-test] Firefox APIs validated successfully");
  }

  private setupTestGlobals() {
    // Vitestライクなグローバル関数を定義
    globalThis.describe = (name: string, fn: () => void | Promise<void>) => {
      this.currentSuite = {
        name,
        tests: [],
        passed: true,
        duration: 0,
      };

      const startTime = performance.now();

      try {
        const result = fn();
        if (result instanceof Promise) {
          result
            .then(() => {
              this.finalizeSuite(startTime);
            })
            .catch((error) => {
              this.handleSuiteError(error, startTime);
            });
        } else {
          this.finalizeSuite(startTime);
        }
      } catch (error) {
        this.handleSuiteError(error, startTime);
      }
    };

    globalThis.it = globalThis.test = (
      name: string,
      fn: () => void | Promise<void>,
    ) => {
      if (!this.currentSuite) {
        throw new Error("test() must be called within describe()");
      }

      const startTime = performance.now();

      try {
        const result = fn();
        if (result instanceof Promise) {
          result
            .then(() => {
              this.addTestResult(name, true, performance.now() - startTime);
            })
            .catch((error) => {
              this.addTestResult(
                name,
                false,
                performance.now() - startTime,
                error.message,
              );
            });
        } else {
          this.addTestResult(name, true, performance.now() - startTime);
        }
      } catch (error) {
        this.addTestResult(
          name,
          false,
          performance.now() - startTime,
          error.message,
        );
      }
    };

    globalThis.expect = (actual: any) => ({
      toBe: (expected: any) => {
        if (actual !== expected) {
          throw new Error(`Expected ${expected}, but got ${actual}`);
        }
      },
      toEqual: (expected: any) => {
        if (JSON.stringify(actual) !== JSON.stringify(expected)) {
          throw new Error(
            `Expected ${JSON.stringify(expected)}, but got ${JSON.stringify(actual)}`,
          );
        }
      },
      toBeTruthy: () => {
        if (!actual) {
          throw new Error(`Expected truthy value, but got ${actual}`);
        }
      },
      toBeFalsy: () => {
        if (actual) {
          throw new Error(`Expected falsy value, but got ${actual}`);
        }
      },
    });

    // Firefox固有のテストヘルパー
    globalThis.expectFirefoxAPI = (apiName: string) => ({
      toBeAvailable: () => {
        if (typeof globalThis[apiName] === "undefined") {
          throw new Error(`Firefox API ${apiName} is not available`);
        }
      },
    });
  }

  private finalizeSuite(startTime: number) {
    if (this.currentSuite) {
      this.currentSuite.duration = performance.now() - startTime;
      this.currentSuite.passed = this.currentSuite.tests.every((t) => t.passed);
      this.testSuites.push(this.currentSuite);
      this.reportSuiteResult(this.currentSuite);
      this.currentSuite = null;
    }
  }

  private handleSuiteError(error: any, startTime: number) {
    if (this.currentSuite) {
      this.currentSuite.duration = performance.now() - startTime;
      this.currentSuite.passed = false;
      this.addTestResult("Suite Error", false, 0, error.message);
      this.testSuites.push(this.currentSuite);
      this.reportSuiteResult(this.currentSuite);
      this.currentSuite = null;
    }
  }

  private addTestResult(
    name: string,
    passed: boolean,
    duration: number,
    error?: string,
  ) {
    if (this.currentSuite) {
      this.currentSuite.tests.push({ name, passed, duration, error });
    }
  }

  private handleVitestMessage(data: any) {
    switch (data.type) {
      case "run-tests":
        this.runTests(data.files);
        break;
      case "stop-tests":
        this.stopTests();
        break;
    }
  }

  private async runTests(testFiles: string[]) {
    console.log("[noraneko-test] Running tests:", testFiles);

    for (const testFile of testFiles) {
      try {
        // Vite経由でテストファイルを動的import
        await import(`http://localhost:5181/${testFile}`);
      } catch (error) {
        console.error(
          `[noraneko-test] Failed to load test file ${testFile}:`,
          error,
        );

        // エラーをテスト結果として記録
        this.testSuites.push({
          name: testFile,
          tests: [
            {
              name: "Module Loading",
              passed: false,
              error: error.message,
              duration: 0,
            },
          ],
          passed: false,
          duration: 0,
        });
      }
    }

    // 全テスト完了を報告
    this.reportTestCompletion();
  }

  private stopTests() {
    console.log("[noraneko-test] Stopping tests...");
    // テスト停止処理
  }

  private reportSuiteResult(suite: TestSuite) {
    const message = {
      type: "test-suite-result",
      suite: suite,
    };

    if (
      this.vitestConnection &&
      this.vitestConnection.readyState === WebSocket.OPEN
    ) {
      this.vitestConnection.send(JSON.stringify(message));
    } else {
      // HTTP経由で結果送信
      fetch("http://localhost:5181/__vitest_runner_result", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(message),
      }).catch(console.error);
    }
  }

  private reportTestCompletion() {
    const summary = {
      type: "test-completion",
      totalSuites: this.testSuites.length,
      passedSuites: this.testSuites.filter((s) => s.passed).length,
      totalTests: this.testSuites.reduce((sum, s) => sum + s.tests.length, 0),
      passedTests: this.testSuites.reduce(
        (sum, s) => sum + s.tests.filter((t) => t.passed).length,
        0,
      ),
      duration: this.testSuites.reduce((sum, s) => sum + s.duration, 0),
    };

    console.log("[noraneko-test] Test completion:", summary);

    if (
      this.vitestConnection &&
      this.vitestConnection.readyState === WebSocket.OPEN
    ) {
      this.vitestConnection.send(JSON.stringify(summary));
    } else {
      fetch("http://localhost:5181/__vitest_runner_completion", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(summary),
      }).catch(console.error);
    }
  }
}

// テストRunner初期化とエクスポート
const testRunner = new FirefoxChromeTestRunner();

export default async function initTests() {
  await testRunner.initialize();

  // 初期化完了をVitestに通知
  console.log("[noraneko-test] Test Runner ready for test execution");

  return testRunner;
}

// vitest.config.ts (修正版 - Custom Pool使用)
import { defineConfig } from "vitest/config";
import { createFirefoxChromePool } from "./vitest-firefox-chrome-pool.ts";

export default defineConfig({
  test: {
    // Custom Pool を使用（Cloudflareパターン）
    pool: "./vitest-firefox-chrome-pool.ts",
    poolOptions: {
      "firefox-chrome": {
        firefoxExecutable: "firefox",
        viteDevServerPort: 5181,
        timeout: 30000,
      },
    },

    // テストファイルのパターン
    include: ["src/**/*.test.ts", "tests/**/*.test.ts"],

    // グローバル設定
    globals: true,

    // テストセットアップ
    setupFiles: ["./test-setup.ts"],

    // レポーター設定
    reporter: ["default", "json"],

    // タイムアウト設定（Firefox起動時間を考慮）
    testTimeout: 30000,
    hookTimeout: 30000,
  },

  // Vite 7 Environment API設定
  environments: {
    client: {
      // デフォルトのクライアント環境
    },
    ssr: {
      // SSR環境（テスト実行に使用）
    },
    firefox_chrome: {
      // カスタムFirefox Chrome環境
      dev: {
        createEnvironment(name, config) {
          // Firefox Chrome環境用のDevEnvironment作成
          return createFirefoxChromeDevEnvironment(name, config);
        },
      },
    },
  },

  // Vite設定を継承
  server: {
    port: 5181,
  },

  plugins: [
    // テスト支援プラグインを追加
    createTestSupportPlugin(),
  ],
});

async function createFirefoxChromeDevEnvironment(name: string, config: any) {
  const { DevEnvironment } = await import("vite");

  return new DevEnvironment(name, config, {
    hot: true,
    transport: {
      send: (data) => {
        // Firefox Chrome環境にメッセージ送信
        console.log(`[vite-env] Sending to Firefox: ${JSON.stringify(data)}`);
      },
      on: (event, handler) => {
        // Firefox環境からのメッセージハンドリング
        console.log(`[vite-env] Registering handler for: ${event}`);
      },
      off: (event, handler) => {
        // ハンドラー解除
      },
    },
  });
}

function createTestSupportPlugin() {
  return {
    name: "firefox-chrome-test-support",
    apply: "serve",
    configureServer(server) {
      // Firefox Chrome環境との通信エンドポイント
      server.middlewares.use("/__firefox_health_check", (req, res) => {
        res.setHeader("Content-Type", "application/json");
        res.end('{"status":"ok"}');
      });

      server.middlewares.use("/__firefox_health_report", (req, res) => {
        if (req.method === "POST") {
          console.log("[vite-plugin] Firefox environment ready");
        }
        res.end("OK");
      });

      server.middlewares.use("/__firefox_execute_tests", (req, res) => {
        if (req.method === "POST") {
          let body = "";
          req.on("data", (chunk) => (body += chunk.toString()));
          req.on("end", () => {
            const command = JSON.parse(body);
            console.log("[vite-plugin] Test execution command:", command);
            // テスト実行処理
            res.end("OK");
          });
        }
      });

      server.middlewares.use("/__firefox_send_message", (req, res) => {
        if (req.method === "POST") {
          let body = "";
          req.on("data", (chunk) => (body += chunk.toString()));
          req.on("end", () => {
            const message = JSON.parse(body);
            console.log("[vite-plugin] Message from Firefox:", message);
            // メッセージ処理
            res.end("OK");
          });
        }
      });
    },
  };
}

function createFirefoxChromeDevEnvironment(name: string, config: any) {
  // DevEnvironment実装
  // 詳細は次のartifactで実装
}

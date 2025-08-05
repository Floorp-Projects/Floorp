// vitest-firefox-chrome-pool.ts
import { type ProcessPool, type Vitest } from 'vitest/node'
import type { Pool, RunWithFiles } from 'vitest'
import { ModuleRunner } from 'vite/module-runner'

interface FirefoxChromePoolOptions {
  firefoxExecutable?: string
  profilePath?: string
  viteDevServerPort?: number
  timeout?: number
}

export class FirefoxChromePool implements ProcessPool {
  public name = 'firefox-chrome'

  constructor(ctx:Vitest) {
    this.ctx = ctx;
    this.options = ctx.config.poolOptions?.["firefox-chrome"] as any;
  }
  
  private firefoxProcess: Deno.ChildProcess | null = null
  private moduleRunner: ModuleRunner | null = null
  private rpcConnection: WebSocket | null = null

  async runTests(files: RunWithFiles[], invalidates?: string[]): Promise<void> {
    console.log('[firefox-pool] Starting Firefox Chrome environment for tests...')
    
    // 1. Firefox環境初期化
    await this.initializeFirefoxEnvironment()
    
    // 2. Vite Environment APIとの連携
    await this.setupViteEnvironment()
    
    // 3. RPC通信チャネル確立
    await this.establishRPCConnection()
    
    // 4. テスト実行
    try {
      await this.executeTests(files)
    } finally {
      await this.cleanup()
    }
  }

  private async initializeFirefoxEnvironment(): Promise<void> {
    const firefoxExecutable = this.options.firefoxExecutable || 'firefox'
    const vitePort = this.options.viteDevServerPort || 5181
    
    // Firefox起動（既存の開発環境起動方式を利用）
    const command = new Deno.Command(Deno.execPath(), {
      args: ["-A", "./tools/dev/launchDev/child-browser.ts"],
      cwd: "../../../..",
      env: {
        ...Deno.env.toObject(),
        NODE_ENV: "test",
        VITEST_MODE: "true",
        VITE_DEV_SERVER_PORT: vitePort.toString()
      },
      stdin: "piped",
      stdout: "inherit",
      stderr: "inherit",
    })

    this.firefoxProcess = command.spawn()
    
    // Firefox起動完了を待機
    await this.waitForFirefoxReady()
  }

  private async setupViteEnvironment(): Promise<void> {
    // Vite 7のEnvironment APIを利用してFirefox Chrome環境を設定
    const viteServer = this.ctx.server
    
    if (viteServer && viteServer.environments) {
      // カスタムFirefox Chrome環境を作成
      const firefoxEnvironment = viteServer.environments['firefox-chrome'] = 
        await this.createFirefoxChromeEnvironment(viteServer)
      
      // ModuleRunnerを初期化
      this.moduleRunner = new ModuleRunner({
        transport: {
          async invoke(data) {
            // Firefox Chrome環境にモジュール実行を委譲
            return await firefoxEnvironment.hot.handleInvoke(data)
          }
        },
        hmr: false // テスト時はHMR無効
      })
    }
  }

  private async createFirefoxChromeEnvironment(viteServer: any) {
    // DevEnvironment for Firefox Chrome
    const { DevEnvironment } = await import('vite')
    
    return new DevEnvironment('firefox-chrome', viteServer.config, {
      hot: true,
      transport: {
        send: (data) => this.sendToFirefox(data),
        on: (event, handler) => this.onFirefoxMessage(event, handler),
        off: (event, handler) => this.offFirefoxMessage(event, handler)
      }
    })
  }

  private async establishRPCConnection(): Promise<void> {
    return new Promise((resolve, reject) => {
      try {
        // Firefox Chrome環境とのWebSocket RPC接続
        this.rpcConnection = new WebSocket('ws://localhost:5183/vitest-rpc')
        
        this.rpcConnection.onopen = () => {
          console.log('[firefox-pool] RPC connection established')
          resolve()
        }
        
        this.rpcConnection.onmessage = (event) => {
          this.handleRPCMessage(JSON.parse(event.data))
        }
        
        this.rpcConnection.onerror = reject
      } catch (error) {
        // WebSocket失敗時はHTTP Pollingにフォールバック
        console.warn('[firefox-pool] WebSocket failed, using HTTP polling')
        this.setupHttpPolling()
        resolve()
      }
    })
  }

  private async executeTests(files: RunWithFiles[]): Promise<void> {
    // Firefox Chrome環境でテスト実行を指示
    const testCommand = {
      type: 'execute-tests',
      files: files.map(f => f.filepath),
      config: {
        timeout: this.options.timeout || 30000
      }
    }

    if (this.rpcConnection && this.rpcConnection.readyState === WebSocket.OPEN) {
      this.rpcConnection.send(JSON.stringify(testCommand))
    } else {
      // HTTP経由でテスト実行を指示
      await fetch('http://localhost:5181/__firefox_execute_tests', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(testCommand)
      })
    }

    // テスト結果を待機
    await this.waitForTestResults()
  }

  private handleRPCMessage(message: any): void {
    switch (message.type) {
      case 'test-result':
        this.processTestResult(message.data)
        break
      case 'test-error':
        this.processTestError(message.data)
        break
      case 'test-completion':
        this.processTestCompletion(message.data)
        break
    }
  }

  private processTestResult(result: any): void {
    // Vitestのテスト結果形式に変換してレポート
    this.ctx.reporter.onTaskUpdate?.(result)
  }

  private processTestError(error: any): void {
    console.error('[firefox-pool] Test error:', error)
    // エラーをVitestに報告
  }

  private processTestCompletion(summary: any): void {
    console.log('[firefox-pool] Tests completed:', summary)
    // 完了をVitestに報告
  }

  private async waitForFirefoxReady(): Promise<void> {
    // Firefox起動とchrome_root.tsの初期化完了を待機
    let attempts = 0
    const maxAttempts = 30

    while (attempts < maxAttempts) {
      try {
        const response = await fetch('http://localhost:5181/__firefox_health_check')
        if (response.ok) {
          console.log('[firefox-pool] Firefox Chrome environment ready')
          return
        }
      } catch {
        // まだ準備中
      }
      
      await new Promise(resolve => setTimeout(resolve, 1000))
      attempts++
    }

    throw new Error('Firefox Chrome environment failed to start')
  }

  private async waitForTestResults(): Promise<void> {
    // テスト結果の完了を待機する仕組み
    return new Promise((resolve) => {
      const checkCompletion = () => {
        // 完了条件をチェック
        // 実装: RPC経由またはファイルベースでの完了通知
        setTimeout(checkCompletion, 100)
      }
      checkCompletion()
    })
  }

  private sendToFirefox(data: any): void {
    if (this.rpcConnection && this.rpcConnection.readyState === WebSocket.OPEN) {
      this.rpcConnection.send(JSON.stringify(data))
    }
  }

  private onFirefoxMessage(event: string, handler: Function): void {
    // Firefox環境からのメッセージハンドリングを設定
  }

  private offFirefoxMessage(event: string, handler: Function): void {
    // メッセージハンドラーを解除
  }

  private setupHttpPolling(): void {
    // WebSocket接続失敗時のHTTP Pollingフォールバック
    setInterval(async () => {
      try {
        const response = await fetch('http://localhost:5181/__firefox_poll_messages')
        if (response.ok) {
          const messages = await response.json()
          messages.forEach((msg: any) => this.handleRPCMessage(msg))
        }
      } catch {
        // ポーリングエラーは無視
      }
    }, 100)
  }

  private async cleanup(): Promise<void> {
    console.log('[firefox-pool] Cleaning up Firefox Chrome environment...')
    
    if (this.rpcConnection) {
      this.rpcConnection.close()
    }
    
    if (this.firefoxProcess) {
      try {
        const writer = this.firefoxProcess.stdin.getWriter()
        await writer.write(new TextEncoder().encode("q"))
        writer.releaseLock()
      } catch (error) {
        console.warn('Error stopping Firefox:', error)
        try {
          this.firefoxProcess.kill("SIGTERM")
        } catch (killError) {
          console.warn('Error killing Firefox:', killError)
        }
      }
    }
  }

  async close(): Promise<void> {
    await this.cleanup()
  }
}

// Pool factory function
export default (ctx: Vitest): Pool => {
  return new FirefoxChromePool(ctx)
}
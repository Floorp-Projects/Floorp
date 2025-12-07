# 調査: Floorp における Chrome Offscreen API のサポート

**日付:** 2025-12-07  
**ステータス:** 調査完了  
**優先度:** 中  
**複雑度:** 高

## 要約

本ドキュメントでは、Chrome拡張機能の互換性向上のため、FloorpにおけるChrome Offscreen APIのサポートの実現可能性を調査します。Offscreen APIは、拡張機能が非表示のバックグラウンドコンテキストでDOMベースの操作を実行できるようにするもので、画像処理、音声/動画処理、またはDOM解析などを行う多くのChrome拡張機能にとって重要です。

## Chrome Offscreen API とは

Chrome Offscreen API（Chrome 109、Manifest V3で導入）は、拡張機能がオフスクリーンドキュメント（ブラウザウィンドウに表示されることなくバックグラウンドで実行されるWebページ）を作成・管理できるようにします。このAPIは、ChromeがMV3で背景ページからサービスワーカーへ移行する際に導入されました。

**主な使用例:**
- **DOM操作**: サービスワーカーでは利用できないDOM API（DOMParser、Canvasなど）へのアクセスが必要な拡張機能
- **メディア処理**: 音声/動画のエンコード/デコード、画像処理
- **サードパーティライブラリの統合**: DOMコンテキストを必要とするライブラリの実行
- **WebRTC**: DOM APIを必要とするリアルタイム通信機能
- **IndexedDB操作**: DOMコンテキストでより良く動作する複雑なデータベース操作

**APIメソッド:**
```javascript
// オフスクリーンドキュメントを作成
chrome.offscreen.createDocument({
  url: 'offscreen.html',
  reasons: ['CLIPBOARD'], // または 'DOM_SCRAPING', 'BLOBS' など
  justification: 'HTMLコンテンツの解析が必要'
});

// オフスクリーンドキュメントを閉じる
chrome.offscreen.closeDocument();

// オフスクリーンドキュメントの存在を確認
chrome.offscreen.hasDocument();
```

## Floorp の現状

### 現在の実装状況

Floorp には堅牢なChrome拡張機能互換性レイヤーがあります:
- **モジュールパス**: `browser-features/modules/modules/chrome-web-store/`
- **主要コンポーネント**:
  - `ChromeWebStore.sys.mts`: Chrome拡張機能サポートのメインモジュール
  - `ManifestTransformer.sys.mts`: Chromeマニフェストを Firefox 形式に変換
  - `Constants.sys.mts`: パーミッションリストと設定
  - `CRXConverter.sys.mts`: CRXファイルをXPIに変換
  - `CodeValidator.sys.mts`: 互換性を検証

### Offscreen API の現状

**場所**: `browser-features/modules/modules/chrome-web-store/Constants.sys.mts:95`

```typescript
export const UNSUPPORTED_PERMISSIONS = [
  // ... その他のパーミッション
  "offscreen",  // 現在はサポート対象外としてマークされている
  // ... その他のパーミッション
] as const;
```

**影響**: `offscreen` パーミッションを要求する拡張機能は、変換時にそれが削除されるため、機能に問題が生じます。

## Firefox/Gecko API との比較

### Firefox の同等API

Firefox には Chrome の Offscreen API と直接同等のものは**ありません**。ただし、関連するAPIやアプローチがいくつかあります:

#### 1. バックグラウンドページ（MV3では非推奨）
```javascript
// manifest.json (MV2)
{
  "background": {
    "page": "background.html",
    "persistent": false
  }
}
```

**ステータス**: 
- ✅ MV2で利用可能
- ❌ MV3では利用不可（サービスワーカーのみ）
- ⚠️ Offscreen APIとは異なる（常時ロード、オンデマンドではない）

#### 2. バックグラウンドコンテキストでの非表示iframe
拡張機能はバックグラウンドページ内に非表示iframeを作成して、同様の機能を実現できます:

```javascript
// バックグラウンドスクリプト内
function createHiddenFrame(url) {
  const iframe = document.createElement('iframe');
  iframe.style.display = 'none';
  iframe.src = browser.runtime.getURL(url);
  document.body.appendChild(iframe);
  return iframe;
}
```

**制限事項**:
- MV2のバックグラウンドページでのみ動作
- MV3のサービスワーカーでは利用不可
- ライフサイクル制御が限定的

### Firefox MV3 の状況

Firefox の MV3 実装（Firefox 128以降）:
- ✅ イベントページをサポート（MV2のバックグラウンドページに類似）
- ✅ DOM アクセスを持つバックグラウンドスクリプト
- ❌ Offscreen API なし
- ⚠️ Chrome の純粋なサービスワーカーアプローチとは異なる

**重要な違い**: Firefox MV3 のバックグラウンドスクリプトはDOMアクセスを持つため、Offscreen API は Chrome ほど重要ではありません。

## ChromeXPIPorter の調査

### ChromeXPIPorter について

ChromeXPIPorter は @FoxRefire による、Chrome拡張機能（CRX形式）を Firefox 拡張機能（XPI形式）に変換するツール/ライブラリです。マニフェスト変換とAPI互換性を処理します。

### 推測される実装アプローチ

外部アクセスがブロックされているため、このようなツールの一般的なパターンに基づくと、ChromeXPIPorter は Offscreen API を以下のように処理していると考えられます:

#### アプローチ1: バックグラウンドページを使用したPolyfill
```javascript
// chrome.offscreen API を Polyfill
if (!chrome.offscreen) {
  chrome.offscreen = {
    createDocument: async ({ url, reasons, justification }) => {
      // バックグラウンドページに非表示iframe作成
      const iframe = document.createElement('iframe');
      iframe.style.display = 'none';
      iframe.src = browser.runtime.getURL(url);
      document.body.appendChild(iframe);
      // クリーンアップのため参照を保存
      window.__offscreenDocument = iframe;
    },
    
    closeDocument: async () => {
      if (window.__offscreenDocument) {
        window.__offscreenDocument.remove();
        window.__offscreenDocument = null;
      }
    },
    
    hasDocument: async () => {
      return !!window.__offscreenDocument;
    }
  };
}
```

#### アプローチ2: マニフェスト変換
MV3 マニフェストを Firefox のイベントページパターンに変換:

```javascript
// Chrome MV3
{
  "manifest_version": 3,
  "background": {
    "service_worker": "background.js"
  },
  "permissions": ["offscreen"]
}

// Firefox MV3 に変換
{
  "manifest_version": 3,
  "background": {
    "scripts": ["background.js"],
    "type": "module"
  }
  // "offscreen" パーミッションを削除
}
```

## Floorp への実装提案

### オプション1: Polyfill ライブラリ（推奨）

**アプローチ**: Firefox の機能を使って Offscreen API をエミュレートする Polyfill スクリプトを注入します。

**実装手順**:

1. **Offscreen Polyfill モジュールの作成**
   - 場所: `browser-features/modules/modules/chrome-web-store/polyfills/OffscreenPolyfill.sys.mts`
   - バックグラウンドコンテキストで非表示 iframe を作成する Polyfill を実装

2. **マニフェスト変換の更新**
   - `offscreen` パーミッションを削除せず維持
   - バックグラウンドコンテキストに Polyfill スクリプトを注入
   - Polyfill されたAPIを示すメタデータを追加

3. **定数の更新**
   - `UNSUPPORTED_PERMISSIONS` から `"offscreen"` を削除
   - 新しい `POLYFILLED_PERMISSIONS` 配列に追加

**メリット**:
- ✅ 拡張機能開発者に透過的
- ✅ 既存のChrome拡張機能で動作
- ✅ 手動のコード変更が不要
- ✅ 類似のAPI表面を提供

**デメリット**:
- ⚠️ MV2 または Firefox の MV3（バックグラウンドスクリプト付き）でのみ動作
- ⚠️ Chrome と比較して若干の動作の違い
- ⚠️ ライフサイクル管理の複雑さ

**推定工数**: 中（2-3週間）

### オプション2: コード変換

**アプローチ**: `chrome.offscreen` API 呼び出しを使用する拡張機能コードを自動的に書き換えます。

**メリット**:
- ✅ 出力をより制御可能
- ✅ Firefox パターンに最適化可能

**デメリット**:
- ❌ 複雑なAST解析が必要
- ❌ ミニファイされたコードで動作しない可能性
- ❌ 保守が困難
- ❌ 拡張機能を壊すリスク

**推定工数**: 高（4-6週間）

**ステータス**: 推奨しない

### オプション3: ドキュメント化 + 手動対応

**アプローチ**: 制限事項を文書化し、移行ガイドを提供します。

**メリット**:
- ✅ 実装の労力が最小限
- ✅ バグのリスクなし

**デメリット**:
- ❌ ユーザーエクスペリエンスが悪い
- ❌ 互換性が低下
- ❌ 拡張機能がそのまま動作しない

**推定工数**: 低（1-2日）

**ステータス**: フォールバックオプションのみ

## 推奨事項

### 即座のアクション（フェーズ1）

1. **✅ Offscreen API Polyfill の実装**（オプション1）
   - Polyfill モジュールの作成
   - マニフェスト変換の更新
   - 定数の更新
   - ビルド統合の追加

2. **📝 Polyfill 動作の文書化**
   - ユーザー向けドキュメントの作成
   - 既知の制限事項の文書化
   - 移行例の提供

3. **🧪 テストスイートの作成**
   - Polyfill の単体テスト
   - サンプル拡張機能との統合テスト
   - 実世界の拡張機能テスト

### 将来の強化（フェーズ2）

1. **🔍 Firefox 開発の監視**
   - Firefox の MV3 実装を追跡
   - ネイティブ Offscreen API の可能性を監視
   - 必要に応じて Polyfill を適応

2. **🎯 Polyfill の改善**
   - より良いライフサイクル管理を追加
   - エラーハンドリングの改善
   - パフォーマンスの最適化

### 長期戦略（フェーズ3）

1. **🤝 Firefox への貢献**
   - Firefox に Offscreen API を提案
   - 実装経験を共有
   - API の標準化を支援

2. **🔄 互換性の維持**
   - Chrome の変更に合わせて Polyfill を更新
   - Firefox の互換性を継続的に確保
   - 新しい使用例のサポート

## リスク評価

| リスク | 深刻度 | 可能性 | 軽減策 |
|------|----------|------------|------------|
| Polyfill が既存の拡張機能を壊す | 高 | 低 | 徹底的なテスト、段階的なロールアウト |
| パフォーマンス問題 | 中 | 低 | ベンチマーク、必要に応じて最適化 |
| Chrome API の変更 | 中 | 中 | Chrome リリースを監視、Polyfill を更新 |
| Firefox MV3 の変更 | 中 | 中 | Firefox 開発を追跡 |
| 保守負担 | 中 | 中 | 良好なドキュメント、クリーンなコード |
| ユーザーの混乱 | 低 | 低 | 明確なドキュメント |

## 成功指標

- ✅ Polyfill がコア Offscreen API を正常に実装
- ✅ Offscreen API を使用する拡張機能の少なくとも80%が正しく動作
- ✅ 重大なパフォーマンス低下なし
- ✅ 肯定的なユーザーフィードバック
- ✅ 拡張機能互換性に関する苦情の減少

## 参考資料

### 公式ドキュメント

- [Chrome Offscreen API ドキュメント](https://developer.chrome.com/docs/extensions/reference/api/offscreen)
- [Firefox WebExtensions API](https://developer.mozilla.org/ja/docs/Mozilla/Add-ons/WebExtensions)
- [Firefox Manifest V3 移行ガイド](https://extensionworkshop.com/documentation/develop/manifest-v3-migration-guide/)

### 関連作品

- ChromeXPIPorter by @FoxRefire (GitHub: FoxRefire/ChromeXPIPorter)
- Mozilla の webextension-polyfill
- ブラウザ拡張API互換性テーブル

### 内部参照

- `browser-features/modules/modules/chrome-web-store/` - Chrome拡張機能サポート
- `browser-features/modules/modules/chrome-web-store/Constants.sys.mts:95` - パーミッションリスト
- `browser-features/modules/modules/chrome-web-store/ManifestTransformer.sys.mts` - マニフェスト変換

## 結論

Chrome Offscreen API のサポートを Floorp に実装することは、Polyfill アプローチによって**実現可能であり、推奨されます**。Firefox にはネイティブの同等物はありませんが、Polyfill はバックグラウンドコンテキストで非表示 iframe を活用することで、ほとんどの使用例に十分な互換性を提供できます。

推奨される実装（オプション1: Polyfill ライブラリ）は、以下のバランスが最適です:
- ✅ 既存の Chrome 拡張機能との互換性
- ✅ 透過的な開発者エクスペリエンス
- ✅ 妥当な実装工数
- ✅ 保守性

Firefox の MV3 への異なるアプローチ（純粋なサービスワーカーではなく、DOM アクセスを持つバックグラウンドスクリプト）を考えると、多くの拡張機能は Firefox で Offscreen API を必要としない可能性があります。しかし、Polyfill を提供することで、Chrome から移植された拡張機能の最大限の互換性を保証します。

**推奨される次のステップ:**
1. 実装アプローチの承認
2. 詳細な技術仕様の作成
3. Polyfill モジュールの実装（スプリント1）
4. マニフェスト変換の更新（スプリント1）
5. テストスイートの作成（スプリント2）
6. 実際の拡張機能でのベータテスト（スプリント2）
7. ドキュメント化とリリース（スプリント3）

**タイムライン見積もり:** 完全な実装とテストで3-4週間
**必要なリソース:** 1人の開発者、パートタイムのQAサポート
**優先度:** 中（互換性を向上、ブロッキングではない）

# Experiments（実験）ドキュメント

このフォルダには、Floorp の A/B テスト / 実験ランタイムに関する設計・使い方・テスト手順をまとめています。

主な内容

- `experiments-client.md` — クライアント実装（`browser-features/modules/.../Experiments.sys.mts`）の詳細、API、pref キー、統合手順、テスト方法。

目的

- 実験の割当と variant 設定のフェッチを安全に行うためのランタイム仕様を一元化します。
- 開発者がローカルで検証しやすい手順と、運用時に必要な注意点を明示します。

参照先

- 実装ファイル: `browser-features/modules/modules/experiments/Experiments.sys.mts`（クラス形式）

次のステップ

- `experiments-client.md` を読んで、起動時の組み込み（BrowserGlue など）とローカル検証手順を試してください。問題があれば、該当箇所のパッチを作成します。

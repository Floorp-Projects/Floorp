# 設定・Welcome 移行の確認手順

2026-09-14。実装とローカル検証は進んでいるが、リリース判定は未完了。詳細な結果は [検証記録](settings-welcome-design-migration-results.md)、当初の要件と段階は [移行計画](settings-welcome-design-migration-plan.md) を参照する。

## 現在の成果物

- `libs/ui/` に共通トークン、Provider、操作部品、Dialog、フォントとライセンスを配置。
- Settings と Welcome の既存入口・ルート・7ステップ・更新案内・通知選択を維持して移行。
- Deno の通常ビルド出力は両ページの `_dist/`。Settings は Vite、Welcome は import map に従い Rolldown を利用する。
- ローカルのページアーカイブは `_dist/design-migration/packaged-pages-final-20260914/chrome/noraneko.jar`。これは完成ブラウザーのインストーラーではない。
- 現在の差分はレビュー用PRとして提出する。リリースは行わない。最新のWelcomeの構成・検証範囲は検証記録の「Welcome の最終調整」を参照する。

## 要件と判定

| 要件 | 現在の根拠 | 判定 |
| --- | --- | --- |
| v4 ボードとブランド仕様 | 公式ロゴ、色、書体、単色面、レールを実装。Windows と Linux の実画面を目視比較 | 実装・代表画面確認済み |
| React 上の共有 UI / Next.js 不要 | Chakra / Emotion / CSS Modules、共有操作部品、既存 Vite 配信 | ビルド・動作確認済み |
| オフライン配信・CSP | JarMaker による manifest 処理、全エントリーのバイト比較、既存 CSP での実描画・フォント読込 | ページアーカイブで確認済み |
| テーマ・翻訳・狭幅 | light/dark/system、OS 相当変更への追従、日本語/英語、315px、200% 拡大、RTL の英語 fallback | 実施範囲は確認済み。全翻訳は未判定 |
| Settings の画面と検索 | 全ルートの表示点検、42項目とセクション検索、対象入力へのフォーカス・直リンク | 実施範囲は確認済み |
| Settings の保存・編集 | 外観、Workspaces、パネル、ジェスチャー、ショートカットの保存・復元。失敗/再試行は対象フォームとエディターのフィクスチャー | 全設定・全失敗経路の網羅は未判定 |
| Welcome の全フロー | 7ステップ、WhatsNew、単独通知選択、実 Actor による検索エンジン選択・完了 | 実施範囲は確認済み。言語パック追加とOS既定化は未検証 |
| キーボード・読み上げ | フォーカス、Escape、入力ラベル、見出し、forced-colors 等 | スクリーンリーダー実機は未検証 |
| OS 別 | Windows 155、Ubuntu/WSL2 headless Linux 154 | macOS と通常 Linux デスクトップは未検証 |
| 完成配布物 | 既存 runtime とソース由来の about/Actor 登録を使った隔離試験 | 完成インストーラーの初回起動・更新は未検証 |
| サイズと性能 | entry サイズ・フォント容量を記録。フォントは各ページ約5.44MB | 起動時間の移行前後比較、容量のリリース判断は未完了 |
| 後続の構成改善 | 7ステップを保持。4群への短縮とナビ再分類は当初計画の後続案 | 今回の初期 UI 移行とは別に検討が必要 |

## ローカルで再現するビルド

リポジトリルートから依存関係を用意した上で、各ページのディレクトリで実行する。

```powershell
# browser-features/pages-settings
deno run -A vite build --config vite.config.ts --base chrome://noraneko-settings/content

# browser-features/pages-welcome
deno run -A vite build --config vite.config.ts --base chrome://noraneko-welcome/content
```

両ページとも500kB超のチャンク警告は残る。共通 UI の strict 型検査は成功。ページの型検査には依存ライブラリー側の型不整合を除外する `skipLibCheck` を使用しており、全依存型の検証成功とは扱わない。

両ページの `test/` にある9モジュール（テーマ、検索、クラス結合、メモリ設定、ショートカット移行・入力方針・URL操作、タブ設定、ジェスチャー保存）を集約し、実 Floorp 内で既存の custom test harness を実行して成功を確認した。これはユニットテストであり、上記の実機フローの代わりにはしない。

ページアーカイブの SHA-256: `47fc99badd43b465e33d7393d38d8a0c0a98af6f44718760533324a3d574edcf`。

## 実機で残る確認

通常利用のプロファイルではなく、削除可能な新規プロファイルを使用する。

1. 対象 OS の完成パッケージで初回起動し、Welcome の7ステップを進む。言語・テーマ・検索エンジン・通知選択が再起動後にも残ることを確認する。
2. 言語パックを追加して表示言語を変更する。OS の既定ブラウザー化は試験専用 OS 環境で実行する。
3. `about:hub` の各ページで値を変更し、再読み込み・ブラウザー再起動後の値を照合する。PWA の作成・名前変更・コンテナー・削除は試験用アプリで確認する。
4. スクリーンリーダーでスキップリンク、見出し、設定名と説明、現在値、エラー、確認ダイアログを読む。Tab/Shift+Tab・矢印・Escape で操作し、重複読み上げとフォーカス喪失を確認する。
5. macOS と Linux デスクトップで、light/dark/system、OS テーマ変更、狭幅、長い翻訳と200%拡大を確認する。
6. 更新後の WhatsNew と単独通知選択を開き、キャンセル・確定・タブ終了の動作を確認する。

結果には OS、runtime とパッケージの版、ページ entry のファイル名、操作、期待結果、実際の結果を残す。未実施項目を合格扱いにしない。

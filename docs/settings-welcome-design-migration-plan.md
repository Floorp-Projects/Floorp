# 設定・Welcome のデザインシステム移行計画

作成日: 2026-09-14。状態: 実装・検証中。以下は当初計画で、現在の実装と検証の記録は `settings-welcome-design-migration-results.md` にまとめる。
調査対象: Floorp checkout `87641b4e6155e`、ローカル web-v5、Blog。

## 1. 結論

React で動く両ページへ、新しい Floorp のデザインシステムを移植できる。
推奨構成は React + TypeScript + Chakra UI v3 + CSS Modules。Deno / Vite / feles-build、React Router、i18next、既存のブラウザー連携は維持する。

ただし「Blog と web-v5 が同じ UI スタック」という前提は補正が必要。

| 対象 | 確認した構成 | 移行への意味 |
| --- | --- | --- |
| web-v5 | React 19.1 / Next 15.5、通常 CSS と CSS Modules。package.json に Chakra / Tailwind はない | ブランド仕様とレイアウト契約の正本 |
| Blog | React 19.1 / Next 15.5、Chakra UI v3 / Emotion、通常 CSS | Chakra のブランドトークンと Provider が実装の出発点 |
| Settings / Welcome | React 19.2 系、Vite 7、Tailwind 4 / daisyUI 5、React Router 7、i18next | React の移行は不要。UI とスタイルを段階的に置き換える |

Blog の `src/theme/system.ts` はブランド色・書体を定義しているが、アプリ向けフォーム・状態・ダークテーマ一式が完成していることを意味しない。web-v5 の資料にも汎用 Button / Selector 未抽出、状態仕様不足、light-only が明記されている。

## 2. ブランドをアプリへ適用するルール

採用する基礎は Split Canvas / 可変する境界。濃紫 `#21113F`、ブランド紫 `#5309E8`、淡紫 `#F7F5FF`、白いコントロール面、罫線、Inter / Noto Sans JP 系の書体を共通化する。

- 単色面、整列、余白、罫線を基本にする。背景の光彩、ぼかし、ガラス、全面グラデーション、装飾図形、カードの反復を整理する。
- 分割は「選択と詳細」「説明と対応する実画面」の関係がある場所に使う。Web の Hero 比率・大見出し・一画面高はアプリ共通値にしない。
- Settings は設定行を読み比べやすい密度にする。Welcome は導入文と関連する選択・画像の余白を広くする。同じ色と操作部品を使いつつ、画面の役割で密度を変える。
- 操作領域は最低 44 × 44px。小さなスイッチの見た目とヒット領域は分ける。ライト面は紫、濃紫面は黄色の 3px focus outline を基準とする。
- disabled / loading / saving / invalid / selected / destructive / restart-required をアプリ用の共通仕様として追加する。保存失敗には理由と再試行手段を示す。
- light / dark / system を初期実装範囲とする。Web の濃紫のブランド面と OS のダークモードは別の概念として設計する。dark 配色は新規拡張として明記し、コントラストを検証する。
- `prefers-reduced-motion`、forced-colors、200% 拡大、長い翻訳、RTL を扱う。文書の lang / dir は現在の言語へ合わせる。
- フォント・ロゴ・画像はパッケージ内から利用できる構成にする。書体の同梱範囲・サイズは測定して決め、オフライン時のフォールバックも用意する。

## 3. UI ライブラリの選定

第一候補は Blog と同じ Chakra UI v3。基本操作と複合フォームは Chakra を基礎とした Floorp 用コンポーネントにまとめ、面・レール・画面レイアウトは CSS Modules が担当する。

| 選択肢 | 評価 | 方針 |
| --- | --- | --- |
| Chakra UI v3 + CSS Modules | Blog のテーマ資産を利用でき、フォームと状態表現を統一できる。Emotion の実行時スタイル生成を実環境で確認する必要がある | 推奨。先行試作を通過して確定 |
| React + CSS Modules + 必要な headless 部品 | web-v5 に近く、視覚設計の自由度が高い。フォームの共通実装・保守が増える | Chakra が配信・性能条件を満たせない場合の代替 |
| Tailwind / daisyUI を再テーマ化 | 既存との差分は小さいが、Blog の部品実装との共通化は進みにくい | 移行中の互換用途に限定 |

Chakra は Vite 向け公式導入手順があり、Next.js は必須ではない。ただし通常の Vite 対応は Floorp の about / chrome ページでの動作保証ではない。[Chakra Vite guide](https://chakra-ui.com/docs/get-started/frameworks/vite)

トークン、semantic tokens、recipes は Chakra のテーマ API に対応させる。移行中の reset は既存 Tailwind と競合しないよう無効化または限定適用する。[Chakra theming](https://chakra-ui.com/docs/theming/overview)

CSS Modules は Vite の機能で扱える。[Vite CSS Modules](https://vite.dev/guide/features.html#css-modules)

React / React DOM は Floorp の既存バージョンへ揃える。Web 側へ合わせたダウングレードはしない。Chakra の採用バージョンは試作時に固定して lockfile で再現する。

## 4. コード構成案

新規の責務境界は次を目安とする。名前は実装時に既存 workspace 規約へ合わせる。

```text
libs/design-tokens/        ブランド値、semantic tokens、CSS 出力
libs/ui/                   Chakra Provider、recipes、Floorp の共通操作部品
libs/browser-page-client/  共通にできる連携型・adapter 契約
browser-features/pages-settings/src/
  app/                     既存ルートの入口
  features/                機能ごとの UI・状態・設定データ
  layout/                  SettingsShell、ナビゲーション、検索
browser-features/pages-welcome/src/
  app/                     既存ルートの入口
  flows/                   初回設定、更新案内、通知選択
  layout/                  WelcomeShell、進行表示
```

依存方向は `pages → ui → tokens`、`pages の機能 → browser adapter → 既存 RPC / Actor`。共通 UI は Services、ChromeUtils、NR*、設定キーを参照しない。

- `libs/ui` は Button / IconButton / Field / Input / Select / Switch / RadioGroup / Dialog / Tooltip / Status を持つ。ページごとの common フォルダーへのコピーを止める。
- Settings の SettingRow / SettingSection / RestartNotice と Welcome の StepNavigation は、それぞれの用途が安定してから必要な範囲だけ共有する。
- トークンは一つの定義から CSS と Chakra 用値を作る。初期段階では Floorp 内で共有し、参照元 web-v5 の更新日と差分を記録する。リポジトリ間の公開パッケージ化は後続作業とする。
- Blog / web-v5 のローカル絶対パスをビルド依存にしない。両リポジトリの同時変更をブラウザー移行の前提にしない。
- テーマの解決・OS 変更購読は共有可能。どこへ永続化するかは adapter の責務にし、Settings と Welcome の異なる現行動作を無条件に統合しない。
- 既存 dataManager と RPC を adapter の裏に段階的に収める。UI 更新と Actor プロトコルの変更は別の変更単位にする。

## 5. Settings の情報設計

画面はナビゲーション、検索、ページ見出し、設定セクションで構成する。各セクションはカードの箱を重ねず、見出し・説明・罫線・設定行で組む。

ナビゲーションの整理案は「概要」「外観とタブ」「作業環境（サイドバー・ワークスペース・Web アプリ・Floorp OS）」「操作（ジェスチャー・ショートカット）」「パフォーマンス」「アカウント」「更新と製品情報」。これは表示上のグループ案であり、既存ルートと翻訳キーは維持して段階的に適用する。

設定行はラベル、説明、操作、補足状態の順序を統一する。通常設定の保存タイミングは現在の契約を維持し、即時反映・適用が必要・再起動が必要を明示する。複雑なショートカット編集、並べ替え、ジェスチャー編集を汎用設定スキーマへ無理に押し込まない。

狭いウィンドウではナビゲーションを収納し、本文は一列へ変える。Web の 1100px を一律採用せず、44px の操作域と設定本文が共存できる幅で境界を決める。

設定検索は先に構成を整理する。現在の `src/lib/search/index.tsx` は各ページを `renderToStaticMarkup` して本文を抽出しており、Chakra 導入後は Provider 不足やスタイル文字列混入の検証が必要になる。推奨は、ルート・項目 ID・タイトル/説明の翻訳キー・検索語を持つ検索用メタデータへ移すこと。全フォームの自動生成は必要ない。

移行前後で代表的な検索語の結果を比較し、既存の本文検索対象を落とさない。段階移行中は未移行ページの抽出を残し、移行済み項目をメタデータで上書きする。検索結果から該当設定へ移動・フォーカスできるよう安定した項目 ID を設ける。

## 6. Welcome の情報設計

最初の UI 移行では現在の 7 ステップを保持する。
`welcome → localization → features → hub → customize → support → finish`

Shell と進行表示を共通化し、各ステップの「いま行うこと」「選択」「次へ/戻る」を明確にする。クリック可能な li でできている現行進行表示は、ボタンまたはリンクと `aria-current="step"` を使ってキーボード操作可能にする。

説明と関連する画像、選択一覧と詳細には Split Canvas を適用する。言語・テーマ・検索エンジンなどの入力画面は操作を主役にする。支援リンク群は罫線付き一覧として独立させる。

導線短縮は次の変更単位で検討する。案は「開始と言語 → 機能紹介 → 初期設定 → 通知設定と完了」の 4 群。Hub 紹介や支援を任意の補助導線へ移す場合も、既存機能への到達と選択の意味を保持する。初回 UI 移行と同時にステップを削除しない。

`?upgrade=...` の WhatsNew、`?releaseNotes=1` の通知選択は独立フローとして維持する。完了前の通知選択確認、pref 保存の成功/失敗、デフォルトブラウザー化、外部の説明・法的情報への導線を回帰検証に含める。

## 7. 先に把握する実装上の注意点

1. 配信は既存 Vite プラグインによる jar manifest 生成を使う。JS / CSS / フォント / 画像 / 遅延チャンクが本番パッケージから読める必要がある。
2. dev の CSP 緩和で成功しても本番対応の証明にはならない。Chakra / Emotion の style 挿入、Dialog の Portal、focus 復帰を実際の about / chrome ページで確認する。CSP を緩めることで試作を通過させない。
3. Settings と Welcome の RPC は同一実装ではない。前者には bridge 待機と Sidebar 連携があり、後者は別の開発ポート判定と NR* callback を使う。transport の共通化は契約確認後に行う。
4. Welcome の App は render 本体で welcome.page.shown を保存している。フロー分割時は表示済みと設定完了の意味を確認し、副作用を再描画から分離する。
5. Welcome のテーマ読み込みは `1 → light / 2 → dark / 他 → system`、保存は `light → 1 / dark → 0 / system → 2` で一致していない。意図した値を runtime 側と照合し、修正と往復テストを UI 移行前の独立変更にする。
6. Settings の ThemeProvider は class、Welcome は class と data-theme を操作する。両者とも system 変更購読を持たない現行コードだった。新しいテーマ解決器で属性と追従を一元管理する。
7. package.json の依存宣言と使用実績は区別する。next-themes、recharts、cmdk、framer-motion などは import とビルド結果を確認してから残すか削除する。

## 8. 段階的な移行と完了条件

| 段階 | 作業・成果物 | 完了条件 |
| --- | --- | --- |
| 0: 棚卸し | 既存ルート・設定キー・保存契約・検索語・Welcome 分岐の一覧、画面キャプチャ、起動/配信サイズの基準値 | 現行挙動を比較でき、テーマ値の不一致を別課題として扱える |
| 1: 先行試作 | 共通トークン、Chakra Provider、Button / Switch / Select / Dialog、Settings 1画面・Welcome 1ステップ | 本番パッケージ上のオフライン描画、CSS 挿入、Portal、キーボード、light/dark が通る。Chakra 採用を確定 |
| 2: 共通基盤 | UI 部品、アプリの状態仕様、テーマ adapter、検索メタデータへの移行経路 | 旧 UI と共存して reset が競合せず、検索対象と pref の往復が維持される |
| 3: Welcome | Shell、既存7ステップ、WhatsNew、通知選択を順次移行 | 初回・更新・単独通知選択の全経路と完了条件が通る |
| 4: Settings | Shell → 単純なフォーム → 外観/サイドバー/ワークスペース → ショートカット/ジェスチャー等 | 全ルート、検索、保存、再起動通知、複雑な編集の操作が通る |
| 5: 構成改善と整理 | Welcome 導線短縮、設定グループ整理、残存 CSS/依存削除、設計資料更新 | 同じ機能へ到達でき、旧部品と不要依存が両ページから消える |

各段階は小さな PR に分ける。共通部品を一括差し替えする前に代表画面で確認し、その後ページ単位で切り替える。必要なら開発用の画面切替を用意するが、旧新 UI を同時に mount して副作用を二重実行しない。

移行中の Tailwind は既存ページ用に残し、新 UI は CSS Modules / Chakra recipes へ寄せる。両対象から利用がなくなった段階で daisyUI と Tailwind のプラグインを削除する。他の内部ページの依存はこの移行で削除しない。

ロールバックは画面またはフロー単位で旧実装へ戻せる状態にする。UI 移行では pref の名前・値形式を変えない。基盤の回帰がある場合は後続画面の移行を止め、先行試作の条件へ戻って判断する。

## 9. 検証とリリース判定

- 実 Floorp の開発環境とパッケージ版で Settings / Welcome を開く。Mock adapter は UI 作業を補助するもので、連携検証の代わりにはしない。
- 既存の関連テストを実行し、pref 保存・再読込、設定検索、hash 直リンク、ショートカットの競合、ジェスチャー保存、並べ替え、Welcome の完了・通知選択を確認する。
- 新規テストはテーマ値の往復、保存失敗・再試行、検索結果の互換性、初回/更新フロー分岐など、今回変える契約を中心にする。
- Windows / macOS / Linux、light/dark/system、OS テーマ変更、日本語/英語/長い翻訳/RTL を確認する。画面幅は 320–390px、700px、1100px 前後、広いデスクトップと短い高さを含める。
- Tab / Shift+Tab、選択群の矢印操作、Dialog の Escape とフォーカス復帰、200% 文字拡大、reduced motion、forced-colors を確認する。h1 / landmark / skip link / field label / status の重複読み上げを点検する。
- 初回表示時間、JS/CSS サイズ、フォント容量を段階0と比較する。許容幅は測定後、先行試作の採用判断時に固定する。未測定の性能改善や工期は約束しない。
- 型検査・ビルド・既存のブラウザー内テストをリポジトリの手順で実行する。pages の package.json にあるダミー test スクリプトは検証結果に数えない。

## 10. 調査元

ブランド仕様の入口: `E:/floorp-dev/web-v5/docs/design-system/README.md`。
読んだ関連仕様: foundations.md、layout.md、components.md、content-and-accessibility.md、implementation-map.md。

実装確認:

- `E:/floorp-dev/web-v5/package.json`、`src/app/layout.tsx`
- `E:/floorp-dev/Blog/package.json`、`src/theme/system.ts`、`src/components/ui/provider.tsx`、`src/app/globals.css` のブランド定義
- Floorp の両 pages の package.json / deno.json / vite.config.ts / App.tsx / main.tsx / globals.css / ThemeProvider / RPC
- Settings の検索実装と主要部品、Welcome の進行表示・テーマ保存・完了・通知選択

この計画はローカル資料とソース調査に基づく。2026-09-14 に共通基盤・代表画面の先行実装と、実 Floorp の chrome URL 上での Chakra 動作・サイズ検証を実施した。更新された実装範囲、ボードとの比較、検証結果、残る移行は [先行実装と検証](./settings-welcome-design-migration-results.md) を参照。全画面の移行・完成パッケージの検証はまだ完了していない。

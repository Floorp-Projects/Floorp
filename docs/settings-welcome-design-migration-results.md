# Settings / Welcome 移行の実装と検証

2026-09-14。共通基盤、Settings、Welcome の段階移行の記録。実装と実ブラウザーでの検証を進めており、未確認事項は末尾に区別している。

## Welcome の最終調整（PR作成時点）

以下は、この後に続く初期移行の記録を更新する追加結果。初期記録の画面構成・chunk名・アーカイブハッシュは、その検証時点のもの。

- 全画面の分割レイアウトと黒寄りのダークテーマを採用。狭幅では縦配置と選択式ナビゲーションに切り替え、「セットアップ 1 / 7」の文字は削除。
- 開始ページは HTML/CSS のブラウザーイラストを約10秒で再生。狭幅では内部余白を高さに合わせ、ロゴ・検索欄・アイコンが切れないように調整。
- 4機能の説明は操作可能な HTML デモへ変更。ワークスペース、パネルサイドバー、Webアプリ、マウスジェスチャーを約10秒ずつ順に実演し、最後で停止。一時停止・再開・再生し直しに対応し、手動操作で自動進行を停止する。reduced-motion では自動開始しない。
- ワークスペースの選択はネイティブ details / radio で実装。system-principal ページで pointerdown をキャンセルするメニュー部品による debug runtime の assertion を避ける。デモから実際の設定変更・インストールは実行しない。
- 1280×800以上の横長画面ではデモの共通高さに上限を設け、中央配置。「戻る・次へ」は全7ページで画面下部の同じ位置に固定。小型画面は本文スクロールを許容する。
- 未確定の Welcome 支援選択は support を初期値とし、確定済みの値と単独の既存ユーザー向け選択フローは維持する。
- Windows Floorpで4機能の順次再生と最終停止、一時停止・再開、手動割り込み、機能切替時の停止、reduced-motionを確認。8画面サイズ×明暗×4機能の64状態で共通サイズと横はみ出し、対象PCサイズの画面内収まりを確認した。
- PR作成前にSettings/WelcomeのDeno本番ビルドとWelcomeのアプリ型検査を確認。型検査はTypeScript 6.0.3とskipLibCheckを使用。以下に記録した初期移行時の全試験を最終変更後に再実行したという意味ではない。

## 実装範囲

- `libs/ui`: 共通トークン、Chakra / Emotion Provider、Button、Input、ネイティブ Switch / Select、Dialog、セクション、公式ロゴ、テーマ解決。
- Settings: ナビゲーションと検索を含む Shell、狭い画面のモーダルナビゲーション、各設定の共通操作部品、ジェスチャー・ショートカット・サイドパネル・PWA の共通 Dialog。フォーム・表・通知も移行し、daisyUI を削除。設定値と RPC の形式は維持。
- Settings の各ルートを遅延読込へ変更。検索は全ページの静的描画を廃止し、既存の検索セクション定義と翻訳名前空間から索引を作る。部分翻訳では fallback 言語のキーも収集し、表示と同じ t() で解決する。ページの mount や IPC は発生しない。
- Welcome: 既存 7 ステップを保持した Shell と進行表示、開始・言語・機能紹介・Hub・初期設定・完了の画面構成、通知選択の共通部品。更新案内と単独通知選択の分岐も維持。
- Gecko テーマ値の読み書きの不一致を修正。保存後もラジオのフォーカスを保持。言語と dir は初回・更新・単独通知選択に共通適用。

## ボードと実画面の比較

基準はユーザー指定の `E:/floorp-dev/web-v5/docs/design-system/mockups/split-canvas-system-board-v4.png` と最新のテキスト仕様。ボード内の画像生成由来の色コード表記の揺れは、テキスト仕様の値に合わせる。

| 観点 | 反映と確認 |
| --- | --- |
| ロゴ | 公式 SVG を無加工で使用。ギアや代替アイコンによるブランド表現を除去 |
| 色面 | deep / canvas の単色面。代表画面の背景グラデーションと重なったカード表現を除去 |
| 境界 | Welcome のヘッダーは 38%、本文は 42% に境界を置き、8px のレールを一段つなぐ。Settings は操作域を優先して幅264pxのナビに適用 |
| 狭い画面 | Welcome は縦配置と4pxの水平線。進行表示だけ横スクロール。390px幅で本文の横はみ出しなし |
| 文字・操作 | 共通部品の操作域を44px以上、見出しを800に統一。Inter / Noto Sans JP をローカル同梱し、実 Floorp で読み込み確認済み。個別画面全体の監査は継続中 |
| 製品画像 | web-v5 の既存スクリーンショットをフルフレームで使用。色付け・切り抜き・遠近変形なし。過去バージョンの画像のため最新UIの証明には使わない |

最新の代表スクリーンショットはローカルの `_dist/design-migration/review-hub.png` と `review-welcome.png`。最新 Deno ビルドを新しい `packaged-pages-review/chrome/noraneko.jar` にまとめ、そのアーカイブを実 Floorp で読み込み、ライトテーマを撮影して指定ボードと目視比較した。entry は Settings `index-Cwg0BDj5.js`、Welcome `index-DN894xBV.js`。旧検証の `runtime-settings.png`、`runtime-welcome.png`、`runtime-dialog.png` と区別する。

## 検証結果

- 両ページの既存 `test/` 全9モジュールを集約して実 Floorp 内で実行し成功。対象はテーマ、検索、utils、idleMemoryReclaim、keyboardShortcutMigration、keyboardShortcutEditorPolicy、inlineUrlAction、tabWindowBehavior、configPersistence。既存の custom test harness を使い、ページのダミー test コマンドは結果に数えていない。IIFE 化による import.meta の警告はあるが、この試験は開発用 RPC transport を検証するものではない。
- Ubuntu / WSL2 の headless Linux Floorp 154.0 でも最新ページのアーカイブを読み込み、言語 Actor、英語表示、ロゴ・画像、欧文/日本語フォントを確認した。隔離プロファイルへ必要な初期 pref を設定した上で、Workspaces の実保存→再読み込み、Welcome の dark 保存→表示・フォーカス保持も成功。初回の素のプロファイルでは Workspaces の有効化 pref がなく、読込エラー表示となった。完成配布物の初期化を検証した試験ではない。
- Linux で確認ダイアログを Escape で閉じると、フォーカスが本文へ戻る問題を発見。共通 ConfirmModal に初期フォーカスと戻り先を明示し、ボタンの autoFocus に依存しない方式へ修正した。Linux 154 と Windows 155 で、Cancel の初期フォーカスと起点ボタンへの復帰を確認。修正後の共通 UI・Settings 型検査が成功。
- 狭幅の追加確認：実 Floorp のウィンドウ幅とページズームを使い、本文の実測幅315pxで設定12ルートと Welcome の全7ステップにページ全体の横はみ出しがないことを確認。外観は試験用の初期 pref を設定してフォームを有効にした状態で確認した。表の内部スクロールはページ全体の横はみ出しとは区別する。ウィンドウ幅・ズーム・投入した設定は試験後に戻した。
- 保存処理修正後の Deno 本番成果物（Settings entry `index-Cwg0BDj5.js`）を実 Floorp で再確認。隔離プロファイルに既知の初期設定を投入し、外観のダブルクリック閉鎖・別キーの分割ビュー設定が Services.prefs に保存され、再読み込みでも復元されることを確認した。
- ショートカットは既存キーとの競合で保存が無効になること、キャンセルで元の pref が残ること、別キーを記録して保存・再読み込みすると復元されることを実 prefs で確認。ジェスチャーも方向を追加したパターンが保存され、再読み込み後に残ることを確認した。これらの試験では対象キーのユーザー値の有無・型・値を退避し、完了時に復元した。
- 外観フォームの保存を直列化し、主設定の RPC 完了を待ってから保存済みの値を更新するよう修正。読込失敗時は未初期化のフォームを無効化し、保存失敗時は内容と再試行操作を残す。未保存の内容はウィンドウの再フォーカスでも上書きしない。実 Floorp 内のページフィクスチャーで、読込失敗→復帰（初期値を書き込まない）、保存失敗→再フォーカス→再試行成功、短時間の3変更で最大同時書込数1・最終値保持を確認した。RPC は失敗・遅延を与える試験用応答であり、実 pref 保存とは区別する。
- ジェスチャーとショートカットの保存ハンドラーが失敗結果を無視して編集を閉じていたため、成功を待って閉じるよう修正した。失敗・例外はダイアログ内に表示し、入力内容を残して同じ保存ボタンから再試行できる。別の chrome ページで実際の親コンポーネントと共通 Dialog を描画し、false と例外の各応答について、内容保持→再試行成功→閉鎖を確認した。この試験は保存応答のフィクスチャーであり、実 pref への書込みの証明にはしない。修正後の Settings の Deno 本番ビルド、アプリ型検査、diff の空白検査が成功。
- 本番 Builder と同じ `deno run -A vite build --config vite.config.ts --base chrome://noraneko-{settings,welcome}/content` を各ページのディレクトリで実行し、両方成功。Settings は Vite 7.3.3、Welcome はルートの import map により rolldown-vite 7.3.1 を使用するため、Node ビルドだけでは同一経路の検証にならない。
- 上記の `_dist/jar.mn` をローカル Floorp-Runtime の `mozbuild.jar` / JarMaker で処理し、実際の `noraneko.jar` と chrome manifest を生成。Settings 183 / Welcome 152 エントリー、両方の131フォントとOFLを確認し、全エントリーを元のビルドファイルとバイト比較した。これはページアーカイブの検証であり、完成インストーラー全体の生成ではない。
- 隔離 Floorp の chrome registry が両 namespace を `jar:file:...!/nora-content-{settings,welcome}/` に解決することを確認。その経路で日本語検索→入力フォーカス→リロード、Welcome の実検索エンジン保存と完了、通知単独フローを再検証した。Rolldown の Welcome entry `index-DN894xBV.js` で7ステップと WhatsNew の見出し・画像・横はみ出しなしを確認。初回の固定待ち時間では読込前に判定したケースがあり、再読込後に全ステップを確認した。
- 同じアーカイブを使い、隔離プロファイルを一時的にオフラインにして Settings / Welcome の画面、ロゴ、Inter と Noto Sans JP の FontFace 読み込み成功を確認した。オフライン設定は終了時に元へ戻した。
- Settings の13ルートを本番 chrome base の about:hub から新規ドキュメントとして開き、各画面に h1 が一つ、本文の横はみ出しなし、表示された input/select/textarea/button に名前の欠落がないことを DOM で確認。これはスクリーンリーダーの読み上げ品質や条件付きの全入力を検証したものではない。ジェスチャーの色入力2つと実験参加方針のラベルを修正し、色ピッカーを44pxにした。
- Floorp OS の初期状態取得失敗が「読み込み中」のまま隠れていたため、理由と再試行を表示するよう修正。実 Floorp で API 不在時のエラー、再失敗時のフォーカス保持、成功応答のフィクスチャーを渡した再試行後の状態画面を確認。OS サービスの有効化・インストールは実行していない。
- 個別エディター：実 Floorp でショートカット記録中の Escape 入力、記録外での Escape 閉鎖、フォーカス復帰を確認。サイドパネルは隔離プロファイルにテスト用パネルを追加し、URL の実 pref 保存、削除確認のキャンセル、テスト用パネルの削除を確認。削除ボタンの未定義翻訳キーも修正した。
- 狭い画面：実 Floorp の最小ウィンドウでは本文幅500pxとなり、外観・サイドバー・ワークスペース・ショートカット・PWA・パフォーマンス・アカウント・更新設定と WhatsNew にページ全体の横はみ出しなし。Codex のブラウザーでは本文390pxの日本語の外観設定と WhatsNew、ナビ開閉・ショートカット画面への移動、WhatsNew の英語切替を確認。実 Floorp で390pxを確認したという意味ではない。

- 両ページの通常 Vite build、および `--base chrome://noraneko-{settings,welcome}/content/` の本番用 build が成功。500kB超のチャンク警告は残る。
- 専用の headless Floorp テストプロファイルにローカル chrome manifest を登録し、本番 base と元の CSP のまま描画。通常のユーザープロファイルを使用せず、ブラウザーの既定アプリも変更していない。
- Workspaces の既存値表示 → Switch で実 Services.prefs に保存 → リロード後の復元を確認。
- Chakra Portal の確認ダイアログ表示、Cancel 初期フォーカス、キャンセル、起点ボタンへのフォーカス復帰を確認。ワークスペースの初期化自体は実行していない。
- 実 Floorp の検索で、ページタイトルに含まれない `popup` から Workspaces を検索できることを確認。
- 実 Floorp の Welcome で dark=0 の保存、dark の反映、ラジオのフォーカス保持を確認。
- ブラウザー内の custom test harness でテーマ値・往復、翻訳/fallback検索、検索正規化の4ケースが成功。テストは `pages-welcome/test/lib/theme-value.test.ts` と `pages-settings/test/lib/search-documents.test.ts`。
- Codex のブラウザーで日本語、デスクトップと390px幅を確認。検索からWorkspacesへの遷移、ナビの開閉、同じルート選択時の閉鎖、Welcomeのステップ移動、darkへの切替が成功。確認した画面にアプリのconsole error / warnなし。
- 共通UIのstrict型検査は成功。両ページのソース・Vite設定を含む型検査は `--skipLibCheck --ignoreDeprecations 5.0` で成功。共有型をブラウザー専用モジュールから分離し、使用されない RPC 定義と Vite/Rolldown のフック型を整理した。依存ライブラリー内の型宣言には Chakra/csstype、dnd-kit/React の既存の不整合があり、skipLibCheckなしの全体検査成功とは報告しない。検査には別チェックアウトの TypeScript を使用した。

テストの初回起動で Marionette sandbox からの動的 import が debug runtime の ModuleLoader assertion を起こした。テスト用 IIFE をブラウザー内で実行する方式に変更し4ケースの成功を確認した。製品ページのルート遅延読込・Dialog は別途本番 chrome URL で成功している。

## サイズ比較

Node によるローカル本番baseのビルドで再集計。同じ圧縮器による初期entry chunkの比較であり、Deno のビルドレポーターが表示する gzip 値、起動時間、全ルートの合計転送量とは区別する。上記の Deno / Rolldown 成果物は別途アーカイブ化して動作確認した。

| 対象 | 変更前 | 変更後 |
| --- | ---: | ---: |
| Settings 初期 JS | 1781.99 kB (gzip 463.36) | 1429.71 kB (gzip 372.03) |
| Settings 主 CSS | 125.36 kB (gzip 20.50) | 151.07 kB (gzip 45.00) |
| Welcome 初期 JS | 510.31 kB (gzip 145.18) | 591.94 kB (gzip 173.79) |
| Welcome CSS | 100.24 kB (gzip 16.37) | 120.13 kB (gzip 40.52) |

Settings は初期JSが約20%減少。Welcome はChakra基盤追加でgzip約28.63kB増加。CSS は同梱フォント定義を含むため、移行前より増えている。503件あったフォント定義は、全ファイルの可変ウェイト軸100–900を確認して131件へ統合した。フォント本体は変更せず、定義CSSを約374kBから96kBへ削減した。Settings 183ファイル、Welcome 152ファイルの manifest エントリーすべてが実在し、両方にフォントとOFLが含まれることを照合済み。

## 残る移行とリリース前の確認

1. WhatsNew、サイドバー・ショートカット・ジェスチャー・PWA の編集ダイアログは共通基盤へ移行済み。両ページの daisyUI プラグインを削除し、フォーム・表・通知を共通 CSS へ移した。全操作と表示の検証は継続中。
2. Inter / Noto Sans JP は同梱済み（131ファイル、約5.44MB）。OFL と出所・ハッシュを保持し、ライセンスを両 jar manifest へ出力する。現状はページごとのビルドに重複して含まれるため、容量評価が残る。
3. `about:hub` / `about:welcome` の入口と実 Actor の連携は確認済み。隔離プロファイルでリポジトリの CustomAboutPage クラスと対象 Actor をビルド・登録し、既存 CSP の本番ページを開いた。NRI18n の日本語解決、実言語一覧、検索エンジン一覧・既定エンジンの変更、完了時の通知選択保存・Welcome タブ終了・about:newtab への移動が成功。完成インストーラーを用いた試験、言語パックの追加インストール、OS の既定ブラウザー化、全設定の保存は未検証。
4. macOS、スクリーンリーダー、Linux の通常デスクトップ環境での確認が残る。実 Floorp で OS 明暗相当のメディア変更に light→dark が追従し、200%ズーム、reduced-motion、forced-colors を有効にした設定画面の表示と横はみ出しなしを確認した。単独通知選択もキャンセル時の設定保持・確定時の保存が成功。RTL は隔離プロファイルで要求言語を ar、表示文言を英語 fallback にして両ページの方向・横はみ出しなしを確認し、画像も目視確認した。Welcome の重複した言語方向設定を除去した。実アラビア語翻訳や全アクセシビリティを保証する検証ではない。
5. 検索に42項目の静的定義と更新・Floorp OS のセクションを追加。既存セクション検索も維持し、項目リンクの query に対象IDを保存する。読み込み後のスクロール・フォーカス移動を実装し、実 Floorp で日本語の「アイコンの隣」から対象入力へのフォーカスとリロード後の再現を確認した。すべての設定項目を個別検索できるという意味ではない。追加の fallback・リンク検証を含むブラウザーテスト4ケースも成功。

## アセットの出所

追加検証：実 Floorp の元の CSP 下でジェスチャー編集の表示・Escape・フォーカス復帰を確認。PWA は実アプリを操作しないフィクスチャーを使用し、コンテナー・名前変更・アンインストールの各ダイアログ表示、キャンセル、起点ボタンへのフォーカス復帰を確認した（`runtime-pwa-rename.png`）。PWA の OS 側の変更処理を検証したものではない。

同一ファイルであることをバイト比較で確認した。実行時にweb-v5のローカルパスは参照しない。

| 配信先 | web-v5の元ファイル |
| --- | --- |
| `libs/ui/assets/Floorp_Logo_B_Dark.svg` | `public/Floorp_Logo_B_Dark.svg` |
| `libs/ui/assets/Floorp_Logo_B_Light.svg` | `public/Floorp_Logo_B_Light.svg` |
| `pages-welcome/src/app/features/assets/workspaces.webp` | `public/images/screenshots/workspaces-800.webp` |
| `pages-welcome/src/app/features/assets/web-panel.webp` | `public/images/screenshots/web-panel-800.webp` |

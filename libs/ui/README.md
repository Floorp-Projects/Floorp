# Floorp pages UI

Settings と Welcome の段階移行用 UI 基盤。React / Vite 上で Chakra UI 3.37.0 と Emotion 11.14.0 を使う。Next.js には依存しない。

## デザインの基準

- `web-v5/docs/design-system/README.md` とその参照仕様。
- ユーザー指定の `web-v5/docs/design-system/mockups/split-canvas-system-board-v4.png` を構成とブランド表現の基準にする。
- アプリのダーク配色は、承認済みの `output/imagegen/ux-mockups-20260914/*-dark-neutral-v4.png` を反映。本文は黒寄りの `#141416`、入力面は `#202024`、補助文字と罫線はニュートラルグレー。濃紫のブランド面と直角の段差を維持する。生成画像の色を抽出せず、正確な値はトークンを正とする。
- 主要ボタンはライト・ダークとも紫背景・白文字。文字リンクの明るい紫とは用途を分ける。入力枠は装飾用の罫線より明瞭な専用色を使う。
- 色の実値は `tokens.css` に置き、Chakra の定義は CSS 変数を参照する。dark のアプリ用意味色はブラウザー側の拡張。
- ロゴは web-v5 の `public/Floorp_Logo_B_{Dark,Light}.svg` の無加工コピー。ブランドロックアップを別のアイコンやテキストで再構成しない。

## 利用と境界

ページのルートを `FloorpUIProvider` で囲み、globals.css から `page.css` を import する。共通部品をページごとに複製しない。部品の型は `types.ts` に置く。

Button / Input は Chakra の unstyled 部品、Dialog は Chakra の Dialog / Portal を使う。Switch と Select はネイティブ入力で、既存の React Hook Form の ref・change event を保つ。Card 系 export は移行時の互換名で、表示は罫線と余白によるセクション。

`defaultBaseConfig` と `preflight: false` により Tailwind reset と二重に適用しない。両ページの daisyUI プラグインは削除済み。Tailwind の色ユーティリティは共通の意味色を参照し、入力・表・通知・ダイアログの表示は共通ライブラリーが担当する。

このライブラリーには Services / NR* / pref キーを持ち込まない。`usePageTheme` はテーマ解決と OS 変更への追従のみを扱い、永続化は各ページの既存 dataManager が担う。Gecko の content override 値は dark=0 / light=1 / system=2。

## 配信

SVG・CSS・遅延チャンクは既存の Vite / jar manifest 経路で配信する。外部の web-v5 チェックアウトはビルド時に不要。CSP の変更も不要。

Inter / Noto Sans JP の Unicode サブセットを `fonts/` に同梱し、`fonts.css` から読み込む。元ファイルと SHA-256 は `fonts/provenance.json` に記録。131 ファイルの合計は約5.44MBで、現在は各ページのビルドにそれぞれ含まれる。`vite-font-licenses.ts` が OFL ライセンスをビルド出力と jar manifest に含める。インライン化を無効にし、既存 CSP のままローカル配信する。実 Floorp で欧文・日本語のフォント読み込みを確認済み。

検証範囲と次の移行対象は `docs/settings-welcome-design-migration-results.md` を参照。

全131フォントで可変ウェイト軸100–900を確認済み。元のウェイト別定義は同一ファイル・同一Unicode範囲の重複だったため、`fonts.css` では一つにまとめている。フォントバイナリーとUnicode範囲は変更していない。

# Intel non-PGO native 検証の準備

2026-10-17 時点では **準備のみ**。長時間 build の承認は未回答で、新しい Runtime 実行ブランチ・CI run は作成していない。`tools/qa/native-nonpgo/workflow.yml.in` は非アクティブなテンプレートであり、helper fixture の commit を封印してから render する。独立レビューで初版の P1 修正を確認し、静的準備は GO。dispatch は承認未回答の NO_GO。最終レビュー結果と実行許可を別々に扱う。

対象は Runtime `7dd619742b25567e479eeda97ae071d10542ad0b`、元 non-PGO run `38963901545` / attempt 1。元 build は Linux から Darwin への cross build だった。同一ジョブで実機 native 認証も行う案では host が Intel macOS に変わるため、新しい host cohort の検証になる。元 binary の compiled Debug は未検証、元 0/20 の切断原因は UNKNOWN のまま。

| 固定対象 | 内容 |
| --- | --- |
| Runtime | commit 7dd6197、tree 6478a96、upstream 158.0.1 / 1a0cfe2 |
| 既存修正 | 元 upstream/debug patch 30 本を順序・SHA 付きで保持。Fluent/Flasco 等を差し替えない |
| 観測修正 | first cause patch `53a02038…428c2`。元 full host file への適用と patched hash `8f9a7909…43c0` をローカル確認 |
| SDK | 元 SDK artifact 12074136738。ZIP / gzip SHA と CRC、SDK 26.5、49,701 members の safe extraction を消費環境で確認 |
| compiler | Darwin clang 22 の task/run0/hash を固定。実行前に 22.1.8 を要求。元 Linux compiler と binary 同一とは扱わない |
| shared target dependency | ONNX HmSZzgp5TJ-szyUmVMMd7g、WASI SirUGMldT3uoFtKn74I9WQ。元 cache URL prefix に一意一致。実 payload の照合は取得時に必須 |
| Rust / native auxiliary | Rust 1.95.0 の rustc/cargo/std 3 archive と Darwin 補助 5 package を固定。計 11 archive の実測 receipt を最終 proof に残す |
| harness | Floorp cbe45a7、既存 context-menu / GUI / lifecycle / diagnostics の 4 files を SHA 固定。既存 assertion を緩めない |

実行候補は標準 `macos-15-intel` 1 job、hard cap 210 分。cold native build の所要時間・peak resource は未実測で、210 分は成功の見積もりではない。元 cross job の build+package は 9,934 秒だった。公開標準 runner の compute は無料枠対象だが、storage 料金は未確認のため、新しい Actions artifact と cache upload を作らず、既存 job log に小さい allowlisted proof だけを出す。[GitHub runner 仕様](https://docs.github.com/en/actions/reference/runners/github-hosted-runners)、[Actions billing](https://docs.github.com/en/billing/concepts/product-billing/github-actions)

| 経過時間の上限 | 検査・動作 |
| --- | --- |
| 開始前 | exact source/tree/harness/image/Python/attempt、既存 SDK の出所・期限、native input 権限、40GiB 以上の実空き容量を確認。global cleanup や権限付与はしない |
| 35 分 | SDK/tool materialization を 1 owned process group 内で監視し、未完なら停止 |
| 40 / 45 分 | 元 30 patches、source provenance、first-cause patch、Debug/tests/non-PGO config と全 compiler/SDK/ONNX/WASI の実解決 path を照合 |
| 185 分 | fresh objdir から full host TU / XUL を build。`stage-package` まで。DMG / universal / product upload は行わない |
| 190 分 | disposable app のみ ad hoc signing。copy 前後の XUL SHA、署名前後の UUID と全 section hash を結合 |
| 200 分 | actual browser BuildID / Debug / sandbox level 3 を確認。controlled cancel の first cause / PID / kernel exit、既存 20/20 trusted context-menu と実 GUI input を同一 job で要求 |
| 205 / 210 分 | owned native PID と group の終了、証拠出力の余裕 / job hard stop |

40GiB は保守的な事前ガードで、native peak の実測値ではない。GitHub の公称 SSD 割当と実際の空き容量を混同しない。Mozilla は macOS build に少なくとも 30GB の空きを案内している。実空きが不足すれば build 前に fail し、追加領域のために global Xcode や OS 設定を変更しない。[Mozilla macOS build](https://firefox-source-docs.mozilla.org/setup/macos_build.html)

実行中は空き 8GiB、private log 容量、owned group と所有照合した native instance の RSS を監視する。10GiB の RSS ガードは共有 page の重複計上を含む保守的な停止基準であり、実測 peak を保証しない。LaunchServices の実 browser / Shim は group 外なので、UID・実 exe・profile・精密な起動時刻・引数 hash を別途照合し、一致個体だけを回収する。回収を要した正常終了、所有不明、未終了、証拠欠落は合格しない。

最初の失敗は memory と atomic exclusive file に固定し、cleanup や後続 collector で置換しない。skip / missing / timeout / attempt > 1 は fail。failure evidence は固定 field と固定 enum の build diagnostic に限定し、profile・nonce・cookie・raw log・signed URL は出力しない。環境 allowlist は継承の制限であり、same-UID に対する強い OS 秘密境界とは主張しない。CI 内で Agent は動かさない。

独立した controlled cancel は `causeReason=launch-cancelled`、connectedBeforeDisconnect=true、認証 PID 一致、kernel exit を要求する。terminal reason は実値を allowlist で記録し、非同期 callback の順序は固定しない。この試験の成功から元 timeout の原因修正や製品 qualification を推論しない。

実行条件は、最終静的レビュー、immutable helper / rendered workflow の SHA、trigger containment の再確認、既存 SDK の期限、長時間 build のユーザー承認が揃うこと。承認がなければ Runtime ブランチの作成・push・dispatch は行わない。承認後も環境 preflight で失敗した場合は、その結果を保存して停止し、別 runner・SDK・資格情報・retry へ自動で切り替えない。

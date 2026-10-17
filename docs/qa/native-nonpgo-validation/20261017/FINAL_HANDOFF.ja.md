# Intel non-PGO native 検証の準備完了

2026-10-17 05:53:31 UTC 時点で静的準備は GO。長時間 CI の承認が未回答のため、実行は NO_GO。Runtime の branch 作成・push・dispatch は行っていない。

準備コードは Floorp fixture commit `505f52245287fbb1e60a00c3b60788d0ca91f51e` / tree `fb30e577172a8cddbceef9b492fa2493497b2607` に固定した。20 files を provider から読み戻し、全 SHA256 が一致した。最終の文書追加 commit はこの fixture を変更しない。Runtime default は `7dd619742b25567e479eeda97ae071d10542ad0b`、Floorp main は `576bc4bbca058e4c24dc5bd2c7526d64cedc94cc` のまま。今回の source branch / 将来の Runtime branch の CI run はともに 0。

実行候補は標準公開 `macos-15-intel` の 1 job、hard cap 210 分。新しい Actions artifact、cache upload、製品署名、公証、公開、merge、資格情報作成、本番設定変更はない。disposable app の ad hoc signing のみを予定する。全体の副作用は SIDE_EFFECTS.json と TRIGGER_CONTAINMENT.json に記録した。

固定 workflow は [WORKFLOW.yml.disabled](WORKFLOW.yml.disabled) に保存した。SHA256 は `ff4589f9c8c33a6595cb11bb8f19e2bebb2e332a0d94c3e1e50d57cdf18de0dc`。Runtime に配送する同じ内容を Floorp の `.github/patches/floorp-runtime/native-diagnostics/isolated-intel-nonpgo-workflow.patch` に patch として保存した。いずれも現在はアクティブな workflow ではない。

承認後の最小手順は次のとおり。

1. 上記の source/helper/harness SHA、Runtime default、既存 workflow の trigger、副作用、SDK artifact 12074136738 の出所・期限・210 分以上の余裕を再確認する。承認された予算と条件を記録する。
2. Runtime `7dd619742b25567e479eeda97ae071d10542ad0b` から隔離 checkout を作り、上記 CI patch だけを適用する。差分が `.github/workflows/mac-native-nonpgo.yml` の新規 1 file だけであることを確認し、実行 commit/tree/workflow SHA を保存する。build 用の source は workflow 内で別 checkout し、元の 30 patches と first-cause patch を順序・hash 付きで適用する。
3. その commit で `codex/mac-native-nonpgo-20261017` を初回作成する。これが承認後に 1 job を起動する唯一の予定操作。初回 push / run attempt 1 を強制する。現在この branch は存在しない。
4. 同一 job 内で固定 SDK / toolchain を検証・展開し、fresh Debug/tests/non-PGO の full host TU、XUL、stage app を build する。実 browser の Debug/BuildID/architecture/sandbox を検証し、controlled cause probe、既存 trusted context-menu 20/20、実 GUI input、owned process cleanup を要求する。
5. 最初の失敗、未実行、skip、missing、timeout、強制回収を fail として保存し停止する。自動 retry、旧 binary 代用、SDK/runner/資格情報の切替は行わない。公開する proof は allowlist 済みの小さい job log のみ。

portable guard 17 件を独立に再実行して成功した。8 Python files の AST、7 新規 JavaScript snippets の構文、workflow YAML、first-cause patch 適用、SDK 49,701 members の安全なローカル展開を確認した。INDEPENDENT_REVIEW.json の対象は静的準備で、残る P0/P1 指摘は 0。rendered workflow は template の fixture placeholder 1 箇所のみを固定値へ置換した。

Darwin full TU / XUL / native authentication は今回まだ実行していない。40GiB の空き、peak RSS、cold build が 210 分内に終わることは未検証。元 run 38963901545 は Linux からの cross build なので、今回の Darwin host は新しい cohort。元 binary の compiled Debug は UNVERIFIED、元 0/20 の原因は UNKNOWN のまま。Debug qualification / 製品 qualification の合格証拠にはしない。

既存 objdir の再利用は引き続き NO_GO。旧結果の seal は変更せず、Node deprecation 警告と、PGO generate / Debug false / Linux host / source・SDK・link closure 未証明という実質的な不一致を WARNING_ASSESSMENT.ja.md で分けて記録した。

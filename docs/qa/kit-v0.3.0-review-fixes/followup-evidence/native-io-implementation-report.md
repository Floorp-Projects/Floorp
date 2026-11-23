**QA309 Runtime P1-NEW-01 implementation**

担当の qa3_native_io.py と test_qa3_native_io.py を変更しました。最終の担当ファイル全テストは 34 methods / 32.883s / exit 0 / skip 0、timeout・残存 child・外側 wrapper の強制回収はゼロです。独立レビューへ渡す状態で固定しました。commit/push は行っていません。private native proof と publication qualification は UNVERIFIED のままです。

実装では run_owned の public arguments を維持し、command ごとに fork した supervisor の中だけで subreaper を設定します。caller の subreaper 状態、既存の無関係な process forest を変更しません。単なる prior direct-child baseline の拡張では、無関係な child の fork→exit が sampling 間に起きた際の late adoption を識別できないため、command の所有範囲を専用 process に分離しました。

Popen 直後の identity/pipe/selector 初期化から cleanup 保護に入ります。通常 I/O が失敗したら初回 error を保持して drain を止め、TERM→KILL→wait/reap はログ保存から独立して進めます。cleanup の OS error は phase ごとに有限保持し、後続 phase を継続します。正常に回収できても error があれば非成功です。cleanup 最初の scan 前に grace が経過しても KILL へ進みます。pipe/file/selector の close、receipt hash/write の失敗も初回 error を置き換えません。

保存可能な process receipt は初回 error の type/errno/message/phase、追加 cleanupErrors、ownedCleanupComplete を持ちます。例外 object 自体は process 間で共有できないため、caller では built-in type/arguments/errno/filename を再構成します。Python traceback object は IPC に渡しません。reply は JSON、最大 128 KiB、wait と終了処理は有限の grace を持ちます。data stdout の IOBudget 使用量を caller に戻し、bool/負値/後退/上限超過の会計を受け入れません。

supervisor の SIGTERM/SIGINT は cancellation flag として記録し、cleanup 自体を interrupt しません。leader exit 0 後の cleanup 中に SIGTERM が来たケースも非成功になり、子は回収されます。初回 TIMEOUT 等が既にある場合はその stop を保持し、cancelled を併記します。

**固定 bytes**

| ファイル | SHA256 |
|---|---|
| .github/workflows/scripts/qa3_native_io.py | f8005ff48f51d857304b1001e2dfc3477f34edb753dcb3abd3a2ff0663438a55 |
| .github/workflows/scripts/test_qa3_native_io.py | f1bfacc36f844e3f4d0aab51066b41aaab30b58f8c57d2d676ddf2fb40283888 |
| 担当 2-file patch | a66fd61bfd775ef7823a0361f79f6702d2ba3290d2c8545033b3292c3ffea923 |

基準 HEAD は 460106115bc76b026c8bf8dad24df7fcb07f97c2。[snapshot](native-io-frozen-snapshot.json)、[scoped patch](native-io-fix.patch) に結合しています。他担当の mozlog 2 ファイルの dirty status は報告し、変更していません。今回の新規 evidence は native-io 接頭辞のみです。

**回帰と最初の失敗**

元の 18 methods に、15 個の ProcessFailureTests と 1 個の budget accounting regression を追加しました。実 process は自分の小型 Python child のみです。post-Popen snapshot、nonblocking pipe、selector 第 1/第 2 registration、selector select、pipe read/close、stdout ENOSPC・stderr EDQUOT、cleanup-only sink、secondary selector close、transient cleanup EMFILE、detached child、TERM 中の late fork、無関係な prior process と late-adopted descendant の保持、caller subreaper 0/1 の保持、leader exit 後の cancellation、expired TERM grace、caller へ戻る IOBudget を検証しています。

修正前の原版は SHA256 d1d5e86dc8ae63977248847e1bd3192b5267966ec8011aa2390fd486f2e7bd86。最初の新規 8 methods は 10 failures になり、子の残存・初回 error の上書きを検出しました。[first raw](native-io-regressions-first-failure.log)、[first receipt](native-io-regressions-first-failure.json) を上書きせず保持しました。

追加の原版負例は、checkout を書き戻さず、fixed Git blob のローカル隔離コピーを import して行いました。unrelated prior child の detached grandchild が誤って終了される、caller subreaper が 0→1 になる、cleanup の EMFILE が後続 KILL/reap を止める、pipe close failure 後の receipt が保存されないことをそれぞれ検出しました。

- [late-adoption original red](native-io-late-adoption-original-red.json) / [raw](native-io-late-adoption-original-red.log)
- [caller state original red](native-io-subreaper-state-original-red.json) / [raw](native-io-subreaper-state-original-red.log)
- [cleanup signal original red](native-io-cleanup-signal-original-red.json) / [raw](native-io-cleanup-signal-original-red.log)
- [pipe close original red](native-io-pipe-close-original-red.json) / [raw](native-io-pipe-close-original-red.log)

最初の担当全ファイル 33 件の実行は cancellation fixture 1 件が失敗しました。fixture timeout=1 と既存 leader-exit grace=1 が競合し、TIMEOUT が先に保持されたためです。exit=0 と cancelled=true の assertion は通り、stop assertion が TIMEOUT != CANCELLED で失敗しました。fixture timeout を 3 にして、先行 timeout のない cleanup cancellation を確実に選びました。本体の意味変更で failure を隠していません。[first full raw](native-io-all-after-fix-final.log)、[diagnostic](native-io-full-first-failure-diagnostic.json) と [observed-field clarification](native-io-full-first-failure-diagnostic-clarification.json) を保持しました。

その後の 33 件は PASS。最終 static 確認で cleanup grace が最初の scan 前に経過する分岐を保護し、対応する regression を追加した固定版の 34 件もすべて PASS です。[最終 raw](native-io-all-frozen-grace-fix.log)、[最終 receipt](native-io-all-frozen-grace-fix.json) は実行前後の source/test hash 一致も記録します。wrapper は AS soft 512 MiB / hard 32 GiB、CPU 40 秒、FSIZE soft 32 MiB / hard 64 GiB、wall 90 秒です。hard AS/FSIZE は既存 inner prlimit と矛盾しない値とし、新規 actual-process fixtures は memory 128 MiB・logs 1 MiB の小型 command です。環境は credentials/global Git config を含めず、全 cleanup は自分の child birth を照合しています。

**Lint と限界**

適用した [mozlint skill](../../repos/Floorp-Runtime/.agents/skills/mozlint/SKILL.md) の “Run ./mach lint without arguments to lint all modified files” を実施しました。無引数 lint は pnpm-audit 登録時の sparse third_party/node/pnpm-lock.yaml 欠落により exit 1 / BLOCKED です。[full modified log](native-io-mach-all-modified-retry.log)。追加 bootstrap や checkout の拡張はしていません。

担当 2 ファイルの Mach ruff + ruff-format は exit 0 / 0 problems。[targeted log](native-io-mach-targeted-frozen.log)。pinned Ruff 0.16.10 の check/format と git diff --check も PASS。途中の exec-server disconnect は process 作成前に拒否された read-only lint/check 呼出であり、auto approval review の拒否ではありません。接続復帰後に別名で実行し、元の test/raw は保持しました。

fork supervisor は単一 command の logical child ownership と通常の例外/cancellation cleanup を扱います。caller/supervisor の hard SIGKILL/STOP、OS の持続的な signal/reap/IO 不能、worker 喪失、OOM の復旧をこれだけで保証しません。externally provisioned launcher と role/run/attempt job scope の全終了・削除、専用 UID、fresh HOME、readonly controller inputs、credentials/network 分離は引き続き必要です。scope、physical memory、native harness、cold dependencies、private launcher の実配備・実行は今回検証していません。

browser/native build/harness、network/model、有料 CI、workflow dispatch/変更、UID/cgroup/infra 設定、資格情報作成、commit/push/merge/release は行っていません。cgroup/UID/config/provenance 検査、private qualification や publication の既存判定は弱めていません。次はこの固定 2-file の独立レビューと、親担当による Runtime 全体の統合 unit 検査です。

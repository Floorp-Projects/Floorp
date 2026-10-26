# QA引継ぎとhelperレシート読み取りの修正

2026-10-26に再確認した引継ぎ。#2513のQA実装を継続するための記録であり、
全製品の合格やマージ・公開の承認を示すものではない。

## 継続するソース

| Repository | ブランチ | 引継ぎ時の固定SHA |
| --- | --- | --- |
| Floorp | `codex/qa-recovery6-native-monitor` | `a2b5e187678926624531edabab9ba9d65cc1c0a9` |
| Floorp | `codex/qa-native-helper-timing-recovery68` | `2a6862440c39ae54d8b18c4a37804059e73200fd` |
| Floorp-Runtime | `codex/mac-native-firefox45-fresh-recovery64-20261020` | `07675f79b2534b29b46bd8631091e6052b2d5ccd` |
| Floorp-Runtime | `codex/mac-native-helper-timing-recovery68` | `3b28276d3e94ac36f25f458144c66fae79ea9cee` |

今回の差分はFloorpのhelper timingブランチを基準にする。mainへの統合検証は未実施。
Floorp #2889・#2893・#2917・#2918・#2921・#2922、Runtime #86はOpen・Draft。
#2918 → #2921 → #2922の積み重ねを維持する。
過去診断を含む121ブランチの添付一覧は、この引継ぎでは取得していない。

## 現在の実行証拠

- Runtime helper timingの現headは[39544511531、attempt 1](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/39544511531)で成功。
  デコード済みjobログの終端レシートは103テスト成功・skip 0・24測定・cleanup確認を報告する。
  資格は`bounded-helper-diagnostic-only`で、`fullHostQualification=false`、
  `nativeAuthentication=NOT_TESTED`。この新しい修正を実行した証拠ではない。
- fresh Firefox45の現headは[39461757999、attempt 1](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/39461757999)で失敗。
  終端レシートの最初の失敗は`mozilla-shell-build`の`monitor-unavailable`。
  3回のinventory timeoutが記録され、進捗は未観測・project import・ABI bindingで止まる。
  helper timingの成功だけで、この長時間buildの原因解消を判断しない。
- Windows validation `8d8bd81c824e46647a3b167d2f77270bfc363dea`に現headのrun/check記録はない。
  旧headの成功は現headの実行結果へ移さない。
- Windows baseline `c45c2ef9224a90bbbf8a81c4b1d8c01326b19bb9`の
  [39983899024](https://github.com/Floorp-Projects/Floorp/actions/runs/39983899024)は、
  `nativeLastTabWorkspacesEnabled.test.ts`の限定成功。smokeとnative matrixはskip。

## 今回の実装

helperのJSONレシート・時刻ファイル・SHA計算で、FIFOを通常の読み取りで開くと
writerを待ち続け、ファイル種別の拒否まで到達しないことを実プロセスで再現した。
成功レシート出力後にファイルが置換された場合も同じ停止が起きた。

`helper_timing_caller.file_bytes`で非ブロッキングopen後に通常ファイルを確認し、
上限付きで同じdescriptorから読み、前後のidentityを照合する。
privateレシートの所有者・単一link・0600条件を保持し、SHA入力は4 MiB、
時刻ファイルは128 bytesまでに制限する。現行141入力の最大サイズは99,181 bytes。
選択済み103件のリスト・hashと、native timeout・resource limit・資格判定は維持する。

## ローカル検証と残る作業

Linux/Python 3.13の秘密情報なし・ネットワークなし隔離環境で、既存86テストに
5つの回帰・互換性テストを加えた91テストが成功した。
Linux内の選択103件は、Mac専用2件のskipを理由に事前検証を拒否する負の対照であり、
Macの103件成功とは区別する。修正前のFIFO待機と、容量不足の一時領域での初回失敗も保存した。

次に、対応するRuntime workflowのfixture commitと2ファイルのSHA pinを更新する。
Runtimeのinitial-push-only条件と公開前承認条件は保持する。
この修正のmacOS実行、fresh Firefox45の原因解明、Windows現headの検証、
現mainとの統合、最終配布物のQAは引き続き未完了。

# Firefox45 実行記録と再開用入力

選定対象は `xpcom/tests/unit` の登録45件で、Macでの実行対象42件と上流manifest無効3件を区別する。
上流のテスト、assertion、manifest、Runtime source、既存planは変更していない。

## 停止・証拠欠落時の記録

`mozilla_upstream.execution_ledger()` は固定planとinventoryのhash・ケースID・件数を照合し、
`pipeline.py` と `fresh45.py` の最終summaryへ `mozillaUpstreamExecution` を保存する。
各行は固定ID、期待値、観測結果、実行状態、理由だけを持つ。

| 条件 | 有効ケースの記録 | 無効ケース | 成功件数 |
| --- | --- | --- | --- |
| inventory未確認 | 全45件 `UNVERIFIED`、有効数は不明 | 判定しない | 不明 |
| inventory確認後、harness開始前に停止 | 42件 `NOT_EXECUTED` | 3件 `MANIFEST_DISABLED` | 0 |
| harness記録欠落・不完全・整合不良 | 42件 `UNVERIFIED` | 3件 `MANIFEST_DISABLED` | 不明 |
| 完全な終了ログあり | 観測結果を `RECORDED` として保存 | 実際のSKIPを別計数 | ログに基づく |

`processEvidenceComplete` は終了・回収・cleanup・監視の証拠を別に示す。
完全なログがあっても、それだけで製品合格にはせず、`productQualification=false` を維持する。
最初の失敗、監視失敗、実行済み／未実行／不明、上流無効は相互に置き換えない。

Runtime側の再開用workflowは、source pin確認後に不明状態の全45行を生成し、
summaryを読めた直後にケース記録を更新する。後続の別の証拠照合が失敗しても記録を保持する。
summary自体がなければ全45件を未確認とする。過去runの結果は変更しない。

## SDKの明示的な選択

既存 `sdk_source.json` と既定経路を維持し、`sdk_source-20261026.json` を指定した場合だけ新しい由来を使う。
schema 2では、SDKを生成したcommitとコンパイル対象のRuntime commitを区別する。

- artifact: `12624490543`、run: `40259426572`、attempt: `1`。
- producer job: `121368405809`、producer commit: `0ac255739cdd64e100cd6913329247421b0ec2ea`。
- コンパイル対象Runtime: `7dd619742b25567e479eeda97ae071d10542ad0b`。
- 圧縮・展開後のbytesとhash、SDK version `26.5`、canonical treeを照合する。
- 新旧SDKは圧縮hashが異なる。展開前tar bytes全体のSHA-256
  `57aeb200890ba97c382bf450f23f9dab3e0d4696fdf02323a705f82210b087e3` と既存canonical treeは一致する。
- APIのrun head・attempt・producer・expiryが一致しなければ取得しない。

内容scan後にstat情報が同じままbytesが変化する負例に対応し、同じopen descriptorから内容を再読する。
SDK bytesの検証は、Mac build・native認証・製品QAの合格を意味しない。
記録上のartifact期限は2026年10月27日11:31 JSTで、将来のAPI取得可否は実行時に再確認する。

## 事前検証と残る実行

Linux/Python 3.13の秘密情報なし・ネットワークなしの隔離環境で696件を実行し、691成功・5 skip・失敗0。
5件は実Darwinのprocess／libproc検証を要求するため、Mac成功件数へ加算しない。
この696件のモジュールと完全なテストID一覧を `fresh45_preflight_catalog.json` に固定する。
Mac実行側は選択一覧の一致と、全696件成功・skip 0を要求する。

Linux検証の初回はファイルtimestampとsnapshot clockのずれによる失敗があった。
synthetic serializer入力の作成前にfilesystem clockへの到達を待つ処理を追加し、
古いprofileを拒否する実際のfreshness条件は維持した。後続の696件は完走した。
初回の全stdoutは保存名の再利用で失われたため、残るexit・失敗抜粋と後続結果を区別する。

公開用Python verifierそのものを隔離内で実行し、summary欠落・harness前停止・不完全ログ・
終了ログがあるがbuild未認定の4つのsynthetic controlで、45行の保持とexit 1を確認した。
これらは実Firefoxテストの実行結果ではない。

再開用Runtime workflowは `b/qa-fresh45-20261026` の初回branch作成push・attempt 1だけを受け付ける。
Intel Mac、job上限360分、作業上限345分、clockリセット禁止、read-only API権限を維持する。
既存Draftへの配置だけではこのbranchを作成しない。native実行は別途承認を得てから行う。

元のrun `39461757999` は `mozilla-shell-build / monitor-unavailable` のまま。
有効42件の実行、Windows現head、最終配布物、全OSの製品qualificationは引き続き未確認である。

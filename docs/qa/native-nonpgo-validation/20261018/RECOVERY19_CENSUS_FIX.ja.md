run [39126068495](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/39126068495) は Mac の186テスト中185成功・1 error・skip0でした。保存した安全な診断で、`test_checkpoint.CheckpointGuards.test_real_mac_new_owned_process_is_observed` の最初の census が `checkpoint-process-owner-changed` で失敗したことを特定しました。monitorFailure は null、group cleanup と leader reap は成功しました。Runtime checkout・SDK取得・build・upload は未実行、consumer は skip、artifact は0件です。終了証拠は `RECOVERY18_PREFLIGHT_RESULT.json` に保存しました。

census は EUID または RUID の一方が worker と一致する候補を選びながら、BSD 照合時には両方が worker と一致することを要求していました。この条件では、ps と BSD が同じ安定した mixed credentials を返していても停止します。実測で guard の失敗は確認しましたが、対象 process の具体的な UID/RUID 値は記録していないため、元 run の相手が安定した mixed helper だったと断定しません。

修正は、正規化済み ps UID/RUID と BSD の UID/RUID を厳密に比較し、候補の `(pid,startSec,startUsec,uid,ruid)` を準備前 baseline と終了時 census に保持するものです。既存の安定した mixed identity は baseline に入り、候補範囲内の新規 identity・世代置換・資格情報変更は差分として checkpoint を拒否します。両資格情報が worker 以外になり候補範囲から脱落する場合まで検出する保証はありません。census は自然成功境界と所有 group cleanup を補足する読取り専用検査で、任意の異 UID writer 不在の一般的証明ではありません。

BSD の完全長・PID・生成時刻・状態検証、unknown/EPERM の失敗、ps/BSD 間の資格情報不一致の失敗を維持します。census 候補へ signal を送らず、Runner の signal 所有確認と cleanup は変更しません。独立 mock 行列で既存 mixed baseline・新 PID・同 PID 別 birth・同 birth 資格情報変更・EUIDだけworker・permission/inventory不一致を検証しました。

Linux 全188件中178 PASS／Darwin専用10 skip、8.498秒。独立全 suite も同じ件数で8.532秒、ResourceWarning を error として成功しました。独立レビューの blocking/P1/P2 残件は0です。次 workflow は Mac188件全PASS・skip0を取得前に要求します。直列の標準 Intel 2 jobs、checkpoint 最大1件・10 GiB・保持1日、同じ SDK/Runtime/toolchain pins と自然成功 Rust 境界を維持します。実 Mac census 成功・実容量・復元・full host/XUL・native authentication は次の実測で判定します。

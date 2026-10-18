run [39126513633](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/39126513633) は188テスト中187成功・1 error・skip0で、census の `checkpoint-process-identity-unavailable` により取得前に停止しました。monitorFailure は null、group cleanup と leader reap は成功、SDK/build/upload は未実行、artifact は0件です。安全な終了記録は `RECOVERY19_PREFLIGHT_RESULT.json` に保存しました。

今回の差分は診断分類だけです。permission（先行 inventory がworker両UIDかmixedか）、short BSD、invalid identity、invalid state、その他を固定 Failure code に分けます。取得不能を成功やゼロへ変換せず、signal 所有確認・資源監視・cleanup・取得条件は変更しません。worker/mixed は先行 ps inventory の分類であり、拒否対象の出生や実際の BSD 資格情報を証明するものではありません。

Linux 全189件中179 PASS／Darwin専用10 skip、8.553秒。独立6テストと、全6固定コードが公開 unittest receipt に残ることを確認しました。独立レビューの blocking/P1/P2 残件は0です。次の実 Mac gate は189件全PASS・skip0を要求します。権限拒否なら、その失敗を保持して通常の読取り専用 API で契約を満たせるか検討し、認証や権限設定を変更しません。同じ直列2 jobs、checkpoint 最大1件・10 GiB・保持1日を維持します。

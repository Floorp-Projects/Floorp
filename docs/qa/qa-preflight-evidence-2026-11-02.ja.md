# Mac preflight失敗証拠の回収（2026-11-02 JST）

NSS実機run41350068009/attempt1/head4b365e79305ccb7a373bee3d372b21fd99be45fc/job124978125641は、2026-11-01T21:09:23Zにfailureで終了した。初回失敗はcheckpoint-tests / nonzero-exit / elapsed288.24秒である。SDK取得、NSS build/load/stack probe、上流42件は開始していない。

公開artifact13100946669と暗号化artifact13100801979の前後API、repo/run/head/attempt/job、期限、全ZIP bytes/digest、全メンバーを照合した。CMS認証復号とmanifestの全hashは一致し、最終stdoutと公開artifactのprojectionも一致する。固定11パス中job-originとfirst-failureの2件を回収した。preflightドライバー、checkpoint-tests.private.logとmonitor-preflight.jsonは旧allowlistにないため、失敗した個別テスト・完了件数・cleanupは不明である。NSS原因やテストのflakinessを推定しない。

新しい実行の公開ledgerはinventory未確認の45 UNVERIFIEDである。固定source上の有効42件＋無効3件の予定分類、旧run41187440235の42 UNVERIFIED＋3 MANIFEST_DISABLEDと今回の実測を混同しない。旧practicalContinuation、旧45ledger、消費済みworkflowと失敗を保持する。SDK13011079529の最新APIと保存正規全ZIPは一致し、2026-11-13T21:08:48Zまで有効。同一bytesの49,701-member監査を再利用したが、今回のnativeには取得されていない。

## 次の限定診断

同じ失敗の原因を読めるようにするため、別workflow `qa-mac-preflight-evidence-20261102.yml` と専用scriptを追加した。専用handoff branchの直前parent4b365e79からの1回のpushだけを許す。attempt1、macos-15-intel、単一job preflight-evidence、最大20分（実作業15分、preflight step10分）、APIで同run/head/job/runnerを照合する。新たなfull nativeの起動やmanual rerunではない。SDK取得、Mozilla source build、NSS build、scope実行は含まない。

入力fixtureは失敗時と同一のab661c0a8be435efef4a7268bc2de1e1a4848a59、157 helper hashes、824 exact IDs、14 recording controls。テスト期待、skip0、query3秒/3samples、profile30秒、FD/owned groupの検証を維持する。Linuxの既済824件を繰り返さず、新workflow・回収器の隔離12テスト（crypto認証往復と改変拒否、origin/API差替え・rerun拒否、固定パス・SDK/環境不収集、link/FIFO/過大ログ拒否）を成功確認した。初回test setupのOpenSSL設定不足による1失敗も別記で保存し、/etcを持たない隔離環境用にOPENSSL_CONF=/dev/nullを設定して再検証した。

回収する10パスはjob-origin.json、started、started-monotonic、first-failure.json、monitor-preflight.json、monitor-preflight-driver.private.log、checkpoint-tests.private.log、failure-recording-tests.private.log、monitor-preflight.private.log、census-abi-compile.private.logのみ。存在しないファイルをunavailableとして記録し、FD同一性・上限・30秒以内のsnapshotを検証する。既存の固定公開証明書へのCMS AES-256-GCM / RSA-OAEP-SHA256を使用し、raw tarはメモリ内だけ、公開artifactには暗号化bytesと固定manifest/hashのみを残す。任意パス、SDK、環境変数を収集しない。秘密鍵をrunnerや隔離回帰へ渡さない。

終端後に同run/head/attempt/jobのAPI・全artifact/ログを照合し、固定10パスを認証復号して個別失敗と実件数・first failure・cleanupを読む。回収できるまでは原因・修正・Mac42合格を認定しない。必要な修正を確認した後に、別の固有入力と上限を記録してNSS実機検証へ戻る。

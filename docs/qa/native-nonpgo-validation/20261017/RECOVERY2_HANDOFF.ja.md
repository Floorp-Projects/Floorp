# Intel non-PGO native validation — recovery 2

今回の変更は、run 39018277446 が tools-materialize/process-activity-format で終了した計測処理の修復です。失敗した行とフィールドは記録されておらず UNKNOWN として保持します。Apple ps が未知の状態を返し得るというソース上の根拠と、今回実際に起きたフィールドの特定は分けています。

未取得の CPU/RSS は null とし、取得済み部分だけを明示的な下限値に分けました。同じ計測を最大3回、間隔0.25秒で取得し、生存中の所有プロセスを完全に計測できない場合は monitor-unavailable として停止します。失敗記録を先に保存し、所有するプロセス群だけを回収します。

同じ標準 macos-15-intel job の Runtime checkout 前に、短い所有プロセスの生成・終了を使う計測検査を置きました。実際の完全なサンプルを1件以上得ることが成功条件です。この Darwin 実測が成功してから、既存の固定 SDK/toolchains、configure、全体 build、host TU/XUL 証拠、native authentication へ進みます。

コード28 guards は root と独立レビューの両方で成功し、Linux の所有プロセス実測も成功しました。これは Darwin full build または native authentication の成功証拠にはなりません。

- 固定 Floorp fixture: `518a54ea05e406f6e244a6a7d465f9118fe3c7e1`
- コード manifest SHA256: `be8df96187d1830ec5ac02e1d9c74e48b0f855d6de5d5a4b03ecb9fbbeb7fbdf`
- 固定 Runtime base: `7dd619742b25567e479eeda97ae071d10542ad0b`
- 起動前の Runtime execution commit: `d86e63bc621d647efd8389fbf226f841ebb52501`
- execution tree: `241e516eb4f1c537842485403d329c7ce6e8c130`
- 初回 push の対象: `codex/mac-native-nonpgo-recovery2-20261017`
- workflow SHA256: `2e03547cd12a7c5414199661db04c19fff09ca5c36b0cf08da1a45fcc5e17409`
- delivery patch SHA256: `9ed218d935a7f0c6b233b96300817c716b27d66b6e265a9fb94f14504cbb5e51`

run 39016143725 の configure/KeyError と run 39018277446 の計測失敗は、どちらも attempt 1 の失敗として残します。GitHub の rerun は行いません。認証条件、native の機能タイムアウト、保護設定、ビルド対象、PGO を使わない今回の検証範囲は変えていません。

08:28 UTC の指示に従い、人為的な210/195分・累積時間制限は解除済みです。GitHub の6時間上限と、複数の進捗指標、ディスク・メモリ・所有プロセスの必要な保護は維持します。新規 artifact/cache upload、製品署名・公証・公開、merge、新規資格情報、本番設定変更はありません。使い捨て検証コピーの ad hoc 署名だけが既存の許可範囲です。製品 qualification は引き続き未達です。

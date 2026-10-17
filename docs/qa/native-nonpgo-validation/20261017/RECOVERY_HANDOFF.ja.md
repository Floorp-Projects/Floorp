# Intel non-PGO recovery の実行準備

2026-10-17 08:39:09 UTC 時点の記録。ユーザーの 08:28 UTC の指示で、累計 210 分・次回 195 分という人為的制限は撤回した。正常な進捗が続く間は検証を継続する。GitHub hosted job の実行上限は 6 時間なので、workflow と全 Runner 呼び出しを 360 分で結合した。[GitHub Actions limits](https://docs.github.com/en/actions/reference/limits)

固定 helper fixture は `b00f0761850046de58f5186614e0d250973fa5ea`、tree `26fabae0fc1b9f7404fbfd41c74a213dc0ee2b27`。Runtime/source/toolchain/SDK/harness/受入条件は元 fixture と同一。非アクティブな [RECOVERY_WORKFLOW.yml.disabled](RECOVERY_WORKFLOW.yml.disabled) は fixture を固定した配送用 workflow。SHA256 は `9fbc21231626e050131b8765a87c8c3338922ee430c887bd9d66fe907ac2bc10`。

[初回 run](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/39016143725) は失敗のまま保存している。SDK・11 toolchain 資材・30 patches・configure コマンドは成功し、設定照合が KeyError で停止した。full build と native 認証は未実行。今回の実行は別 run / 別 branch として記録し、最終 proof の priorExecution に初回の失敗を結合する。

照合修正は HOST_CPU_ARCH と macOS 最低バージョンの参照に限定する。macOS 10.15 は四つの実 compiler command の単一 `-mmacosx-version-min=10.15` を要求し、存在しない substitution key を前提にしない。欠落・異なる値・競合する値は拒否する。残る必須設定キーと型は固定上流の限定 configure files と照合した。23 guards が独立再実行を含め成功し、コードレビューの阻害指摘は 0。

30 秒ごとの公開 status は stage、経過時間、空き容量、log bytes、owned RSS、CPU 秒、process 数・状態、進捗停止秒数のみ。無出力だけでは停止しない。CPU / log / process roster / 累積 RSS / 累積 disk の進捗が 20 分すべて止まり、runnable process もない場合に stall と判定する。既存のメモリ・ディスク・log 容量・transport・cleanup・native 機能 timeout は維持する。最初の失敗を保持し、自動再試行は行わない。

新しい限定 Runtime branch は `codex/mac-native-nonpgo-recovery-20261017`。Runtime `7dd619742b25567e479eeda97ae071d10542ad0b` / tree `6478a96fe1c1eec9468dd9d631bd065d1128c123` から、`.github/workflows/mac-native-nonpgo.yml` の追加 1 file だけを含む commit を作り、provider readback / 最終 route review 後の初回 branch 作成で起動する。元 Runtime branch と run は変更しない。既存 24 workflows の push match と workflow_run listener は 0。attempt 1 / created true / before zero / exact ref / read-only permissions の guard を維持する。

既存 SDK artifact 12074136738 は 2026-10-17 08:39:09 UTC の supported read で未失効、期限 2026-10-17T20:41:19Z、digest と source run は一致した。CI でも再確認してから取得し、6 時間分の有効期間がなければ停止する。

標準公開 `macos-15-intel` の単一 job、新しい Actions artifact / cache upload は 0。disposable test copy の既存 ad hoc signing だけを使う。製品署名・公証・merge・release・公開、新しい資格情報・設定・権限、有料 runner、Blacksmith 別ログインは対象外。

これから得る Darwin host TU / XUL / native 認証の証拠は新しい Darwin host cohort の検証であり、元 Linux cross binary の compiled Debug や元 0/20 の切断原因を遡って証明しない。製品 qualification は false のまま。過去の 210/195 分を記した文書は当時の immutable 記録として保持し、今回の時間方針はこの記録と RECOVERY_PREPARATION.json を使う。

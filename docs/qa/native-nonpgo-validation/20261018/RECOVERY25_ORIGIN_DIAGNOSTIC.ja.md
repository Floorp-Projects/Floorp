# Recovery25: 元親の診断限定実験

run 39136105689 は checkpoint-owned-survivors で停止した。Rust と archive は exit 0、全 stage group は verified-gone / leader reaped、monitorFailure は null。新規候補13件はすべて identity-verified の metadata-worker、現在の親は PID 1。採取済み stage birth との一致はないが、これは系譜の完全証明ではない。保存artifactは0件、consumerは未実行。

追加する短い macos-15-intel job は SDK取得・Floorpビルド・artifact保存を行わない。資格判定は変更しない。既存 UID census / delta=0 / resource / cleanup 制限を保持する。

通常の proc_pidinfo flavor17、arg=0 の完全長56 bytes、errno=0、UUID/uniqueid/reserved fields を検証する。idversion は signed int32 であり正値前提にしない。候補の BSD 5tuple・PPID・PGID と uniqueid/idversion 等を二重照合する。PID 1 の birth/credentials は同じ primary sysctl の任意診断オプションで取得し、二重照合する。既存 primary call のデフォルト挙動は変わらない。拒否・短長・不安定・欠落は unknown とし、他のAPIへ切り替えない。

管理下の Popen 子が fork した孫を、専用pipeによって制御する。親が生存中、孤児化後、孤児の exec 後を観測する。exec 後に parent uniqueid が PID 1 と一致しても original parent version が元の子のままである負例を必須とする。終了は pipe EOF、30秒の自己終了、直接 Popen 子の回収に限定し、任意PIDへのsignal権限は与えない。group gone を別途確認し、原失敗と cleanup 詳細を保持する。

公開するのは fixed enums / booleans / 件数 / API bytes/errno のみ。raw PID、birth、kernel uniqueid、UUID、version、環境、秘密は公開しない。取得不能はnullのまま保持する。

この診断が成功しても、PID 1 原親整合は非job因果・非writerの証明ではない。カウンタwrap、親execによる不一致もある。分類は pid1-original-parent-consistent / original-parent-not-established に留め、除外・signal・checkpoint資格付与を常にfalseとする。

一次仕様: https://github.com/apple-oss-distributions/xnu/blob/xnu-11417.101.15/bsd/sys/proc_info_private.h 、同tagの proc_info.c / kern_fork.c / kern_exec.c / kern_proc.c。固定ABIを実runnerの既知子・孤児・exec対照で検証する。Mach-O/署名の追加診断は非writer証明に足りないため今回の範囲に含めない。

長時間試行はこの短jobの結果と独立レビュー後に判断する。同じ資格条件のまま無変更再試行はしない。

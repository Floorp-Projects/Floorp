QA309 Runtime supervisor 境界の修正を、担当 2 ファイルだけで固定しました。全 47 methods / 45.898s / exit 0 / skip 0 / OK。実行前後の source/test hash は一致し、outer timeout、外側による強制回収、残存 child はすべてゼロです。Ruff、対象 2 ファイルの mach ruff/ruff-format、git diff --check も PASS。commit/push は行っていません。独立再レビューと Runtime 全体の統合検査は親担当が実施します。

前の固定版 f8005ff4 の 34 件 PASS の後、独立レビューが親側 `_proc(pid)` の EIO、および None の後の reader 初期化 EIO を確認しました。どちらも親が約 12 秒で例外を返しながら supervisor と TERM を無視する command を残しました。EIO ケースでは writer FD も残りました。この P1 は前回の PASS だけでは閉じていませんでした。原ログと前回の snapshot/report は変更していません。

今回の親側回収は pidfd を優先し、取得や通知に失敗した場合も、自分の未回収 fork child を waitid(WNOWAIT) で確認して TERM→KILL→reap を続けます。親の `/proc` 初回 snapshot がなくても終了通知できます。PID の再利用や他者による reap を避けるため、fork 前に単一 thread と default SIGCHLD を要求し、caller の handler/subreaper 状態を変更しません。parent reader/writer/pidfd、fork 自体の失敗、child 側の初期化を保護します。初回 errno/args を保持し、二次 cleanup error は phase ごとの有限な exception note に保存します。error があれば回収できても非成功です。保存可能な command process receipt は child 側の cancellation/cleanup を記録します。

private IPC の別の P2 も修正しました。envelope と success result の key を固定し、error は None または型付き error object のみとします。success result は整数 exit、stop=None、ownedCleanupComplete=True、boolean stdoutIsData、有限 duration、正規 SHA256 を要求します。public call の name/argv/stdoutIsData を照合し、nonzero exit は明示 boolean の allow_nonzero=True の場合だけ受け入れます。SystemExit(0) を再構成しません。型付き monotone staging budget と既存 OSError の errno/filename を維持します。この検査は trusted producer の返答防御です。untrusted native target が control pipe を書けることや、資格判定を突破できることを実証した指摘ではありません。

追加の 13 methods は親 snapshot/reader/writer/pidfd の失敗、pidfd 未提供時の held-child 経路、二次 signal EMFILE、fork/close 複合失敗、16 の malformed envelope subcases、SystemExit(0)、valid result/OSError、allow_nonzero/command binding、budget 型・後退・上限、fork 前の単一 owner guard を検証します。実 process は自分の小型 Python child だけです。前の detached/late-fork/unrelated late-adoption/caller subreaper/cleanup memory・log budget 回帰も全件再実行しました。

新しい RED は [snapshot 初期化](native-io-parent-snapshot-red-01.log)、[None + reader 初期化](native-io-parent-reader-red-01.log)、[16 envelope 負例](native-io-private-envelope-red-01.log)、[SystemExit](native-io-systemexit-red-01.log)、[fork + close](native-io-fork-fd-red-01.log) に保存しました。最初の 4 件は f8005ff4、fork/close は中間版 788f4088 で検出しています。各 JSON に実行対象の hash、外側 limits、回収結果を結合しています。

固定 source SHA256 は `8bbea4abfc5d8cf1a3b70a275287c7fe6725b96fa17afb4c74ef51814b7d142e`、test は `0dae5c088abd51a84338a62c977a7a43e5765dfb1fd8a0647090b10fcab625a6`。基準 commit は `cc83e6fea6499d6ec89c76ae9837c280556e9d53`。[snapshot](native-io-supervisor-boundary-snapshot.json)、[2-file patch](native-io-supervisor-boundary-fix.patch)、[最終 raw](native-io-all-supervisor-boundary-final-01.log)、[最終 receipt](native-io-all-supervisor-boundary-final-01.json)、[対象 mach](native-io-mach-targeted-boundary-01.log) を参照してください。

無引数 mach lint は既知の sparse `third_party/node/pnpm-lock.yaml` 欠落で BLOCKED のままです。[既存 raw](native-io-mach-all-modified-retry.log) を保持し、追加 bootstrap や checkout 拡張はしていません。

外側 AS soft 512 MiB / hard 32 GiB、CPU 40 秒、FSIZE soft 32 MiB / hard 64 GiB、wall 90 秒の既存 fixture wrapper を使いました。hard limit は inner prlimit と矛盾しない値です。今回の新しい command は AS 128 MiB / logs 1 MiB、資格情報なしです。fixture cleanup は自分の birth を確認した process のみです。

hard SIGKILL/STOP、持続的 OS failure、OOM/worker 喪失後の全 forest 回収は外部 launcher/job scope が必要です。専用 UID/cgroup、root-owned readonly inputs、fresh HOME、secret/network 分離、cold dependencies と native harness の実配備・実行は未実証で、private native qualification は UNVERIFIED のままです。機種ラベルや今回の unit PASS はこの条件を満たしません。browser/native/build/network/paid CI、workflow dispatch/変更、UID/cgroup/infra/credentials、merge/release は実施していません。

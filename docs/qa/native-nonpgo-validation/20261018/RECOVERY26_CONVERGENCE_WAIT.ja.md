# Recovery26: strict boundary の bounded read-only convergence wait

run24 は Rust/archive exit0、全stage cleanup成功、monitorFailure=null の後、新規UID/RUID候補13件を checkpoint-owned-survivors で拒否した。候補はすべて metadata-worker / current PPID1 だった。run25の短Mac実験は226/226・skip0で成功し、孤児execがparent uniqueidをPID1由来へ変えても元親versionは残る負例を実測した。別runnerのmetadata16件は元親PID1と整合したが、run24との同一性や非writer性を証明しない。

次に判別する仮説は、自然成功したbuild直後の候補が既存期限内に自然終了し、元の厳密なbaseline差分0へ収束するかである。名前・元親の例外を作らず、候補をsignalせず、baselineを更新しない。変更は即時一回の判定から bounded read-only convergence wait への観測時刻/待機方針だけである。最終資格は元のstrict zero、全stage exit0/group gone/leader reaped、archive等の固定検証を保持する。

待機中は既存Runnerと同じ1秒間隔でprimary censusを取り直す。selfと全new候補（無関係の背景候補も含む）について、BSDのPID/birth/EUID/RUIDを前後照合し、task RSSを合算して10240MiB制限を適用する。測れない候補はunknown/nullで直ちに停止する。独立ESRCHまたは同birth zombieが確認できたgoneのみを件数として区別する。PID再利用・generation-replacedは新生存世代を捨てる根拠にしない。資源測定はatomicな全体snapshotではなく、各query間の競合限界は既存監視と同様に残る。

free disk8GiB、既存private log256MiB/件、native-private128MiB、20分のmultisignal stallを保持する。stallのCPU/RSS/rosterには監視自身を含めず、new候補の実測だけを用いる。monotonic job起点は変更せず、既存scan期限260分、pack305分、upload335分、final345分、platform360分を延長しない。収束待ちはscanの残り時間を消費する。取得不能や残存group、既存failureは即停止する。

全回のnonempty/zero件数とresource測定を保持し、最初と直近16回のdelta詳細を別に保持する。古い詳細を省略した事実を明示し、停止時の最後のreceiptと原失敗を残す。raw PID/birth/kernel IDs/秘密は公開しない。

新testsには16回超、primary/資源unknown、RSS/disk/log制限、monotonic期限跨ぎ、multisignal stall、同birth zombie/ESRCH対generation replacement、前後credential競合、および実Macの保持子→nonempty/RSS実測→pipe EOF/回収→strict zeroを含む。長jobではSDK取得より先に244/244・skip0を必須にする。原native終了原因とfull host/XUL/認証は引き続き未検証である。

既存の2 serial standard macos-15-intel jobs、raw artifact最大1件・10GiB・retention1日、SDKの未失効取得と固定ZIP/TAR/tree/version、製品署名/公開/merge/設定変更なしを維持する。

SDK前に、実Macの既存metadata-worker最大16件について同じ5tuple/task資源APIの代表測定も必須にする。観測0は代表API未確認としてfail、取得不能もfailとし、skipで通さない。この名前による選択は診断限定であり、待機中の全new候補測定とstrictzeroには例外を作らない。

metadata代表gateの失敗はproduction helperの固定Failure literalコードへ分類する。既存のunittest_receiptが非test sourceのliteralを抽出し、monitorPreflight.tests.cases.failureCodeへ安全に投影する。permission / generic query unavailable / no candidate / all gone (no measurement) を区別する。補助のJSON printはprivate test log内に留まり、公開判定には固定分類を用いる。成功には実測metadata候補1件以上を必須にし、self-only/gone-onlyを代表API成功としない。

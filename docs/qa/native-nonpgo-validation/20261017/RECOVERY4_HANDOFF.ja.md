# Recovery4: native 起動診断へ到達するための監視修正

run 39020119793 は全体ビルド・host TU・XUL・ad hoc 検証コピーが成功し、認証前の起動判定で失敗しました。`open` と未捕捉のブラウザーを区別する harness 修正を行いましたが、次の run 39037341780 はビルド途中で ps state の取得不能により停止し、native 検査は未実行です。監視修正を元の host 終了原因とする因果関係は主張していません。両 run の failure と前回の全ビルド成功証拠を保持しています。

今回、Darwin では ps を PGID/PID の列挙に限定し、libproc の task 情報から生成時刻・UID・PGID に結合したメモリー・CPU・実行可能 thread 数を取得します。CPU は実測 timebase で変換します。未知を 0 や消滅に置き換えず、既存の取得回数・メモリー上限・disk/log/時間制限・cleanup を維持します。ビルド前に実 Mac の自分自身と owned process churn で native 測定の成功を必須にします。

native 検証では、launcher と実 browser の exit code/signal、cleanup 前後、設定した引数と実 argv 観測有無を分けます。stdout/stderr の先頭・末尾各32KiBから固定 marker と signed32 OSStatus を記録します。probe の環境は値を含まない flag として記録し、LaunchServices アプリの環境は認証前 UNVERIFIED と明示します。既知の予期しない host 終了は cleanup 中でも失敗になります。起動引数・環境・署名手順・認証条件・sandbox・native 入力要件を変更していません。

root と独立レビューで48 tests 成功。コード fixture は `fa68751b34b6c4d62b5e0ea30e052bbe8e33d603`、実行候補は `9b561d7df526a2cc40d0f4737d141039c713b6df`。13 code ファイルの provider readback、workflow 本文、配送 patch 適用結果を照合し、固定 route は GO です。この文書の時点で新 CI は未起動です。

次は既存承認内で標準 macos-15-intel、新規ブランチの初回 push / attempt1 / 1 job を使います。Runtime C++・mozconfig・SDK・toolchain の固定値は維持し、同一ジョブで実際にビルドした host TU/XUL と native 検査を結合します。元 host の実終了理由と元認証 timeout の原因は UNKNOWN、製品資格と完全な Mozilla Debug qualification は未成立です。

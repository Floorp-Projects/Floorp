# Recovery3: native 起動判定の修正

前回 run 39020119793 / attempt 1 は全体ビルド、実 host TU、XUL のリンク、検証コピーの ad hoc 署名検証まで成功しました。成功証拠は `RECOVERY2_FULL_BUILD_RECEIPT.json` に分離し、元 run の failure と native 未資格の状態を保持しています。

最初の native 失敗 `cause-host-exited` は、認証前に実ブラウザーを特定せず `open` の終了を返した判定です。ブラウザーが実際に終了したか、その理由、元の認証 timeout の原因は UNKNOWN です。

固定 cbe harness への小さな patch で既存の exact-argv 検索を受動利用し、launcher とブラウザーの生存状態を分離します。早期捕捉した identity と Marionette が認証した identity の一致を必須にします。cleanup 前の生存状態・終了コード・要求 signal と固定ログ marker を保存し、診断や cleanup の例外で最初の失敗を置き換えません。35/90 秒の既存起動待ち、認証条件、sandbox、ネイティブ入力要件は維持します。

root と独立レビューはそれぞれ 36 tests 成功。コード fixture は `69391c259358025ff2532d027b970e5babf43ec9`、実行候補は `be9eadeb6deccb3bcb879ce29acf6f9219687171` です。12 code ファイルの provider readback、実 workflow 全文、配送 patch 適用結果を照合し、固定 route は GO です。この文書の時点で新 CI は未起動です。

次の実機検証は既存承認の範囲で、標準 macos-15-intel の新ブランチ初回 push / attempt 1 / 1 job を使います。前ジョブは完了済みで大きな成果物・cache を保存していないため、同一ジョブで実際にビルドした host TU・XUL と native 検査を結合するには再ビルドが必要です。Runtime C++、mozconfig、SDK、toolchain の固定値は変更しません。

製品の署名・公証・公開は行いません。製品資格、完全な Mozilla Debug qualification、全 OS の製品成果物 QA はこの限定検証の成功条件に含めません。

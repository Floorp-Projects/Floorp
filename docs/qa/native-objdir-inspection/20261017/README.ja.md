# 既存 Intel objdir の互換性検査

Runtime `7dd619742b25567e479eeda97ae071d10542ad0b` の既存 PGO-generate objdir 1 個を読み取り、完全な host translation unit の追加検証に利用できるかを調べる。今回の実行は設定・依存資材の検査までであり、コンパイル・リンク・署名・認証試験は行わない。

候補は元の non-PGO 失敗と設定が異なる。仮に読めても、そのまま元の失敗環境の再現や Debug qualification として扱わない。既存の観測修正は原因修正の証明ではなく、元の原因は未解明のままである。

公式 download-artifact は完全 ZIP の digest 不一致を warning として扱うため、ジョブ成功だけでは受け入れない。完了した同一 run / attempt / head のログで実測 digest の完全一致と warning・HTTP error の不存在を別途確認する。検査中は常に incremental build を不許可とする。

外側 ZIP は固定した単一通常ファイル、local / central の名前とサイズ、ZIP64 のみの extra、data descriptor を先に確認する。内側 tar.gz は展開・実行せず読み、CRC・EOF・件数・サイズ・時間・メモリ上限を検査する。既知の設定値のみ型付き要約として公開し、未知値・実行パス・リンク先は hash のみ残す。

実行条件は `EXECUTION_INTENT.json`、補助スクリプトは `.github/qa/native_objdir/`、Runtime 呼び出し側は Floorp に保持する CI patch を参照する。追加の Actions artifact は作成しない。

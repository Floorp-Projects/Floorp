# Mac preflightの3 errorと親ディレクトリ束縛の修正

2026-11-03 JST 00時台。M1継続、製品資格と上流42件は未認定。

修正診断 [41418651939/attempt1/job125188586689](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/41418651939)、head `d35a4f538b9e2f1942d8ea5749aa47de32cc61f5` はfailure。artifact13128574135の認証API前後・期限・repo/run/head/attempt/job、完全ZIP206853bytes/digest・全2メンバー、CMS認証復号・回収8ファイル全hash/manifestを照合した。全157 pinsと固定fixture `ab661c0a8be435efef4a7268bc2de1e1a4848a59` の824 exact IDsが一致した。

824件は207.338秒で完走し、821成功・3error・0failure/skip。3件は `test_nss_runtime.RuntimeTests` の `test_actual_stock_wrapper_roundtrip_with_control_executable_is_bounded`、`test_complete_control_proof_rejects_missing_stage_or_substituted_receipt`、`test_stack_success_without_exact_marker_is_not_accepted`。いずれもstock wrapper実行後の `tool_reader.verify()` で `directory-binding` となった。first failure `checkpoint-tests/nonzero-exit/566.68秒` は保存summaryと一致。ABI compile/ABI/checkpointの3stageのleader reap/owned group消滅を確認した。session/全子孫や全task cleanupまでは認定しない。後続14記録制御・NSS build/load・上流42件は未実行。

入力toolを囲む親ディレクトリへ別の出力を追加すると、元observerがlink countの変化を拒否する。同じ失敗点をLinux隔離環境で実際の兄弟ディレクトリ追加によって再現した。Macで変わった属性そのものは新診断で測定する。元NSS41350068009の個別失敗は回収されておらず、この別実行の結果で書換えない。

`_FixedRead` の祖先ディレクトリではdev/inode/owner/modeとFD/名前の照合を維持し、変動する兄弟entry数を含むnlinkを内容snapshotから外した。読み取るroot・内側のdirectory/fileは従来通りnlink/size/mtime/ctimeを含む完全な同一性を保持し、regular fileの単一link・symlink/FIFO拒否も保つ。root/toolの差替え、ancestorのmode変更、保護tool内のentry追加は拒否する。

関連104件と新しいログ追加観測1件の合計105 unique testsが秘密情報なし/networkなし/QA source read-onlyの隔離環境で成功。追加6件はこの105に含み、別加算しない。全830 exact IDsを発見し、旧824 IDsを全保持。157入力hashのうちobserver/test/catalogの3つだけが変わり、残る154は不変。全830件の再実行や変更なし725件の再加算はしない。

前回Floorp CI41418643334/head523d41d7の全5jobs、Docs41418643241のverify、Runtime shadow41418661594/headd35a4f53/contracts312件は終端success。全artifact/API/完全ZIP/logを照合した。shadowの実行kindはpolicy-preflight-onlyで、製品判定ではない。SDK13011079529の最新APIと保存正規全ZIPは同一、期限2026-11-13T21:08:48Z。同一bytesの49701-member監査を再利用し、この短時間診断ではSDKを取得しない。

次の検証は別workflow `qa-mac-stack-binding-20261103.yml` の初回push1回。修正fixtureと157hashes、830全ID catalog/105実行ID hashを固定し、macos-15-intelで関連105件と未到達14記録制御、owned monitor controlを確認する。job20分/work15分/preflight step10分、API開始基準の時計、query3秒/3samples/profile30秒、skip0とowned cleanupを維持。SDK/本体buildなし、固定10パスのみCMSで回収する。実行済みworkflow/script、45ledger、first failures、旧practicalは保持する。

Macでの修正成功はまだ未確認。この限定検証の終端を照合してから、新source/SDK/API/期限を凍結した別NSS実行へ進む。main/merge/release/required化、manual rerun、routine変更、再委任は行わない。

新診断の起動/API時計/選択105IDs/差替え拒否/失敗保存/CMS認証の隔離18制御と、修正sourceへ結ぶ公開射影7制御も成功した。公開失敗時の45行・first failure・UNVERIFIEDと最終stdout/保存artifact一致を保持。候補は製品資格を付与しない。

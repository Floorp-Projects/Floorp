# Macの入力束縛修正を確認し、NSS実機検証へ接続

2026-11-03 JST 06時台。M1を継続する。上流42件と製品資格は未認定。

限定診断 [41470665560/attempt1/job125363063696](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/41470665560)、head `8068a6435ded47cc63fe0aa1dac1a4d720969615` はsuccess。前後の認証API、repo/run/head/attempt/job、artifact13155740670の期限、完全ZIP63490bytes/digest、全2メンバー、CMS認証・manifest・回収9ファイル全hashを照合した。固定fixture `3c0a7345a47cc94953ceff21a116b9d0f32a8b95` の全157pinsと実行105 exact IDsが一致した。

Macの関連105件は6.060秒で全成功、記録制御14件も0.482秒で全成功、failure/error/skipは0。以前の3directory-binding errorのテストもすべて成功した。新しい制御内で、兄弟出力作成により祖先のnlinkが4→5、size/mtime/ctimeも変化し、inode/owner/mode、保護tool directory/fileの全属性が不変であることを観測した。これは修正後の実Mac制御の根拠であり、元NSS41350068009の回収できなかった個別失敗を確定する根拠にはしない。

owned monitor controlも成功。5stageのleader reap/owned process group消滅・signal権限の失効を確認し、firstFailure/monitorFailureはnull、first-failure.jsonは正しく未作成だった。task全体やsession/全子孫cleanupは未認定。短時間診断はSDK/NSS/上流42件を実行せず、45ledgerと製品判定を生成しない。旧失敗・first failure・ledger・practicalは保持する。

同fixtureの通常Floorp CI41470645627の全5jobs、Runtime shadow41470673236/contracts312件は、全API/artifact/完全ZIP/logsを照合して終端success。shadowの判定はpolicy-preflight-only。SDK13011079529は最新APIと保存正規全ZIPのhash/sizeが同一、期限2026-11-13T21:08:48Z。全49701メンバーと全archive identityの既存監査を同じbytesに対して再利用する。

新しい `b/qa-fresh45-nss-binding-20261103` の初回push/attempt1だけを許す別originを追加する。新しいprofileは `fresh-firefox45-nss-binding-job-origin-20261103-v1`。旧origin/branch/workflowは保持し、選択flagを省略した経路、混在、他ref、rerun、dispatch、既存taskの再利用を拒否する。API開始基準の時計、work345分/job360分、macos-15-intel/fresh-firefox45を維持する。

新しい回収経路だけに固定5パスのpreflight summary・helper全件ログ・14制御ログ・monitorログ・ABI compileログを加え、NSS3パスと元8パスの合計16パスをCMS認証暗号化する。旧8/11パス経路、個別/総量上限、single-link/FD/名前束縛、専用recipient、SDK/任意ファイルの除外は保持する。取得前のtransport probeでも同じ新経路を検証し、失敗時の公開射影・認証回収をalwaysで残す。

新sourceの全839 exact IDsを発見し、旧830 IDs全保持。origin/回収に関する67件が秘密情報なし・networkなし・QA source read-onlyの隔離環境で全成功、skip0。新しい9件はこの67件に含む。157hashes中5ファイルだけ変更し、残る152は不変。全839回帰と変更なし772件はここでは再実行・加算しない。新nativeのpreflightでは全839成功/skip0を要求する。

次は新fixture/head・全pins/catalog・SDK/API/期限、独立公開NSS predicate、公開射影7制御、起動・flag・構文・資源契約を凍結し、同head CI/shadowの終端を照合する。その後にbranch不存在/push0・担当/両Draft・固有操作意図を直前照合し、必要な新nativeの初回1回へ進む。既存runの再送/manual rerun、main/merge/release/required化、routine変更、再委任は行わない。

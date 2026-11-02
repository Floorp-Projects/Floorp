# Mac preflight 診断の時間契約修正（2026-11-02 JST 18時台）

## 確認した終端

固定診断 [41382285951 / attempt 1 / job125075486438](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/41382285951)、Runtime head `7c7d52d3142dfb80dc10645aa23a383c2413c8d6` は failure。03:17:39Z開始、03:23:00Z終了。固定fixtureは `ab661c0a8be435efef4a7268bc2de1e1a4848a59`。

artifact13113693723の認証API前後・期限・repo/run/head/attempt/job・完全ZIP22393 bytes・SHA-256 `a3824efdc5dc8bc3a2079cc9d41a09e56dcade44f7a0e7535ab0fac1c5a92108` を照合。CMS認証復号と全回収file hashes、公開metadataと認証済みmanifestの一致も確認した。10パス中6パスを回収し、残る4パスは未生成である。

診断器は `plan.limits.jobMinutes=360` を15へ置換していたが、既存 `process_census.prepare_abi` は `Runner.run(..., 360, phase_deadline_minutes=15)` を使う。`Runner.run` はplatform引数とplanの一致を要求するため、`platform-budget-binding` でコンパイル・テストのspawn前に拒否された。完全driver tracebackとsummaryのfailurePhase=abi、stages空を照合した。保存first failureは `monitor-preflight / checkpoint-preflight-failure / 312.89秒` で一致する。

この診断の824テスト・14記録制御は実行0。SDK・NSS・上流42件も未実行である。owned stage processは未起動、全task cleanupは未認定。診断には製品公開verifierや45ledgerを生成する工程はない。先行NSS41350068009の `checkpoint-tests/nonzero-exit` と個別失敗不明、公開45 UNVERIFIED/inventory未確認、旧42 UNVERIFIED+3無効、旧practicalContinuationは保持する。今回の診断器の不整合を、先行NSSの根本原因とは扱わない。

## 別の修正候補

実行済みworkflowとscriptは変更せず、`qa_preflight_budget_20261102.py` と `qa-mac-preflight-budget-20261102.yml` を追加した。元のplan limitsを全て保持し、Runnerのplatform契約360分を変えず、API job開始に結ぶ15分のphase期限を設定する。ABIは従来の明示15分、残る各stageも明示15分で、時計はresetしない。実際のjob20分・preflight step10分・work15分、query3秒/3samples、profile30秒、source/FD identity、owned group/reap条件、skip0要求を保持する。360分は既存Runnerの引数契約であり、新jobの実行許容量ではない。

修正診断は専用handoff branchの固定parent `7c7d52d3142dfb80dc10645aa23a383c2413c8d6` から1push、attempt1・macos-15-intel・preflight-evidence 1jobだけを許す。新しいworkflowパスとprofile `mac-preflight-budget-20261102-v2` に結合する。fixture/全157hashes/824-ID catalog/上流assertionsとmanifestは変更しない。SDK取得・Mozilla/NSS buildは行わず、固定10パスを既存の固定公開証明書によるCMS AES-256-GCM/RSA-OAEP-SHA256で回収する。秘密鍵はrunner/source/回帰sandbox/成果物へ渡さない。

## 検証と次の判定

秘密情報なし・networkなし・source read-onlyの隔離環境で16テスト成功、skip/fail/error0。従来のtransport/origin/差替え/暗号認証負例12件に加え、実Runnerによる旧不整合の再現、新契約がnative compile境界へ達すること、API開始から15分経過時のspawn前拒否、first failure保存を検証した。コンパイル境界の先はsynthetic例外で止めており、Mac ABIや実テスト成功の証拠にはしない。shell/埋込Python構文、全157hashes・824 catalog不変も照合した。変更しない既済全回帰は再実行・加算しない。

次は修正した単一診断の終端から個別失敗・件数・監視・stage cleanupを読む。その結果を得てからNSSの別入力/必要な実機検証を判断する。消費済みbranch/runの再利用、manual rerun、main変更、merge/release/required化、再委任は行わない。Mac42件と製品資格は未認定で、M1は継続する。

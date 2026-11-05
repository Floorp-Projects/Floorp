## 依存Draft3046・標準CI・完全製品対照を完了（2026-11-05 UTC）

補足2ファイルとブラウザー回帰試験を [Draft3046](https://github.com/Floorp-Projects/Floorp/pull/3046) head d058d7001b822f0ef2413b1307c98a463dfdcdb9 に提出。固定baseはcanonical2950 head69d306b11c9e52da9e6270870740d49b1d561659の無変更mirror。作者fork/2950は変更なし。2950への依存補足であり代替fix/独立merge対象ではない。

**標準CI成功:** 依存158系run42093984715は267modules成功0失敗。最新main29a3+2950+補足の統合493d2974（159系）run42094068516は284modules成功0失敗。observer宣言7case/settings宣言3caseの両module実行を認証済みartifactで確認（個別成功行とは区別）。統合macOS jobは最初hosted容量不足・runner未取得・steps0で中止。そのjobだけ1回再試行して成功し、Ubuntuは再実行なし。原macOS Photon UI試験ではない。

**完全製品:** 対照main29a3 run42093687541/artifact13446089131 と、統合493d run42094061748/artifact13447726956 が成功。両ZIPをAPI SHA256で認証し安全展開。既存完全production frontend/application/package経路＋互換locked prebuilt native Runtimeであり、生成資産差替copyではない。新規Geckoコンパイル/strict release/helper/XPT/署名配布の合格は主張しない。既存workflowの空補助archive緩和を新たな厳格合格に読み替えず記録。Runtime lock/workflow/製品資産/保護の変更なし。

Debian13.6/通常DBus/Xvfb102 X11、実WebDriver・native shadowbutton合成入力、native159.0.1/BuildID20261103153232、sandbox6。package INI BuildIDは別記録、変更なし。
- 実UIで自然に作った nativefalse/storedvertical と nativetrue/storedhorizontal は対照の通常再起動でも不一致。統合製品は両profileともcold起動で修復。全窓・全タブaboutblank/noHubを記録し、Hub getter読込後だけの改善とは区別。
- 統合製品のHub→native、nativeoff/on→Hub、新窓2窓の実垂直、Workspace往復、multirow・通常再起動保持が両移行profileで成功。今回新規baselineでは新窓/Workspaceを再試験せず、以前の対照証拠と区別。
- 完成品のomni.ja内の実出荷settings ESMをHubから直接importし、実getterの古いform＋外部実pref writer＋実saveで無関係favicon変更を比較。対照2件失敗→統合2件成功（horizontal↔multirow最新style保持、favicon保存/nativefalse維持）。代替source注入/RPC mock/CSP変更なし。2窓の実UI autosave競合タイミング再現とは区別。

計6製品実行、正常終了10回、全製品hash不変、所有process0/display清掃済み。最初のaboutblank Location setter拒否は手順障害として保存し標準WebDriver Navigateで解消。製品バグ/未再現に誤分類しない。

[完全製品・CI・再現script・matrix証拠 commit a411cb5a](https://github.com/Floorp-Projects/Floorp/tree/a411cb5af57b6581667c75f52211ba0fbc18b8ef/docs/qa/additional/package-pwa/KAN-24/followup)

残条件: 原macOS PhotonはMac担当KAN10終了まで未アクセス、実2窓pending autosave timing、多数タブmultirow形状。2950/3046の依存順レビュー・統合待ち。元Linux22分母・元担当7件matrixはbyte不変、KAN7 GPU未充足を保持。進行中を維持し、merge/release/公開/Done化なし。追加ユーザー操作依頼なし。

## 現行LinuxのUI・配置・描画・入力比較（KAN-95 / GitHub2670）

**分類：視覚的な背景余白は観測。現行Linuxで新たな意図しないFloorp欠陥は特定せず。原Windows条件は未検証。**

main29a3eb23145da90706d8a94441373278521cc737、公式Floorp12.21.1/Gecko159.0.1 build20261103153232 と Firefox159.0.1を、Debian13.6/Xvfb101/Openboxで実行。Lepton/Fluerial/Protonは設定画面の可視ラベルをW3Cクリックし、checked・保存値・実描画/寸法を確認。通常/最大化・メニューバー・タイトルバー構成等39配置観測、最大化20件はEWMH両atomとGecko状態を照合。構成fixtureと物理操作は区別し、保護を無効化していない。

タブ外側のクリック領域は上端0px。背景塗りはFloorp各design4px、Firefox6pxから。実Openbox識別XDG_CURRENT_DESKTOP=openboxでnative GTK CSDを有効にした最大化窓では、画面最上端のOS XTestクリックで別QAタブを両製品とも選択し、中央クリック対照も成功した。合成OS入力であり物理入力ではない。

Fluerialのbase CSSはmargin-block0だが、現在の補助FLUERIAL_TAB_CORNER_CSSが「top gapを復元」と明記し、より強いID selectorで4px fallbackを!important指定している。実機CSSOMで補助sheetの読込・selector一致・important値・computed4pxを確認。別の任意8px drag_space設定は未設定で、本結果とは区別。既存current-main製品rebuild（無関係のPR2934 sidebar修正のみ、KAN95修正候補ではない）も1条件同結果。

原報告Windows10/Floorp12.17.0のタイトルバー・窓状態や現行への比較は残る。Linux結果で原報告の未再現/既修正/Doneを断定しない。確認した範囲はpaint余白で、クリック不能な上端隙間は観測せず。推測CSS修正・新PRなし。needs-reproを維持し、次条件をoriginal Windows10/current native titlebarへ絞る。

証拠：KAN-95/report.md、matrix.json、measurement-summary.json、cascade-29963.json、公式製品/Firefox/再buildの各JSON・スクリーンショット。PR2854は別issue2822のtitlebar表示修正で、2670の修正済み根拠にはしていない。

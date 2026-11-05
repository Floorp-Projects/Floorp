# 元配布/PWA7件 — 最新資格・原条件・次作業の統合表

元Linux22分母は22のまま、担当は同じ7件。KAN24は別追加表のまま。Jira全コメントと正本PR最新headを照合し、既済みbuild/基本起動/同一locale試験を繰り返していない。現環境はDebian13.6、user namespace/通常DBus可、Xvfb/nestedKWin/Flatpak可、完全systembus/物理GPUなし。

| Key | 修正PR・CI/完全製品資格 | 今回条件で未再現の範囲 | 次に不足する具体条件 | 利用環境で今回着手した/次作業 | Jira |
|---|---|---|---|---|---|
| KAN-8 | Flathub Draft198 d9aa24d; unit9成功、local完全Flatpak build/install成功。現headのGitHub checks/status0。Flathub署名配布資格なし。 | 元の切替欠陥は公式packageで再現、候補で限定修復。データ消失は未確認。 | 原Bazzite44/Flatseal/Bazaarのpermission/desktop操作 / 完全portal/systembus / 公式Flathub build/signature review / GUI上の拒否通知/任意absolute-path複数profile/同一dir別名等は3最小guard fixtureを超えて未検証 | 今回cache内容・XDG-only既存配置・実launcher3guardを追加実測済み。原Bazzite/署名配布条件は現Debianで代替しない。 | 10308 |
| KAN-17 | 根拠ある製品修正PRなし。公式Floorp159 Flatpak26.08/Firefox159 Flatpak25.08比較、signed-ja実UIは既測。 | Noto可視のDebianでは未再現。BluefinでFirefoxのみ正常という差は未解明。 | 原Bluefin44.20261022.1/Floorp12.20.2@158.0.1のfontconfig/cache / 同一ページ・新規profile（原報告は未確認） / 動くFirefoxの配布形式/runtime/フォント可視性の比較情報 | 原Bluefin側の上記材料が得られればpackage/font差分比較。既済みのNoto-present Debian反復で原条件を解消しない。 | 10300 |
| KAN-23 | Draft3030 e55152c; 標準CI42045064743 282modules/0fail。完全package42067997767・ja/es-CL対照4件成功。 | 元branding/iconは完全baselineで再現、候補で修復確認。en-USだけのCIは言語pack回帰を単独で証明しない。 | 他locale/theme/channel / 保護維持での標準test-mode+active signed langpack / 署名配布・原desktop全体資格 | 標準test-mode helperはsandbox1/CSP無効を生成するため起動せず。保護維持の別harness設計が必要。製品不具合とせず同じcollector timeoutも反復しない。 | 10352 |
| KAN-48 | 3032/3040はidentity関連のみ。CI各281成功、完全製品あり。ただし元のmatching-ID/wrong-iconを直した証明ではない。 | 原Floorp誤iconは当初flat fixtureでも今回実VRTMAXでも未再現。既修正/3032 fix/重複扱いしない。 | 原CachyOS/KDE6.7.5/pacman/報告版profileとfull desktop / VTMGOはprivacy gate転送/no actual manifestで原PWA未資格 / 物理GPU/原permission条件は未検証 | 今回VRTMAXを追加実測。VTMGOのprivacy/原siteflowを同意や認証の突破で置換しない。原OSのmatching-ID/wrong-icon境界へ引継ぎ。 | 10320 |
| KAN-49 | Draft3032 8249e234 +依存3040 2deb165d。標準CI42050337102/42077116984各281/0。完全3040 run42077174018・nativeWayland6条件成功。 | 3032全製品で通常browserのPWA identity継承を再現し3040で修復。原Manjaro/Outlook/Teams全体は未資格。 | 原Manjaro/KDE/Outlook/Teams/taskbar pixels / 全field無効legacyの既存randomidentity限界 / 初回writable product-root lock生成原因未特定（最終fresh6件成功でも抹消しない） | 既存read-only native比較・trace診断・未追跡fresh比較済み。lockの具体的trigger/callerが得られるまで同じ6成功条件や推測修正を反復しない。 | 10378 |
| KAN-89 | 3030はlegacy-branding経路の関連のみ。KAN89のverified fix/duplicateではない。 | 原Firefox logoは未再現。genericX/desktopFileName空は原症状と別の関連付け不足で、Floorp表示合格でもない。 | 原Manjaro/KDE6.7.4/Xorg/Flatpakの正常desktop/portal / KWin desktopFileName/icon association原因 / 原locale/theme/package154系との差 | 正常な原desktop/Flatpak関連付け条件が先決。Exec移設済prefixで同じ3locale反復やKAN23との推測duplicateは行わない。 | 10321 |
| KAN-96 | 既存merged2680 actor修正と整合。完全main29a3 packageで新COOP/COEP2UIinstall+2generatedlaunch成功、追加PR不要。 | 今回の159/制御ページ・実container source・safe-mode制御ページではactor mismatch未再現。CachyOS全体の既修正確定ではない。 | 原CachyOS/Arch package/profile/permission/full desktop / 元safe-mode YouTubeMusicはproxy CONNECT403・実ブラウザー拒否画面。外部cross-site resources未資格 / PWAへのcontainer割当は通常rollout未有効で未資格（source-container検証と区別） / 物理入力/GPU資格なし | 今回COOP/COEP・container source・safe-mode coldflowへ追加実測。実YouTubeMusicは現在のproxy CONNECT403を別記録し、迂回/強制rolloutで合格を作らない。 | 10276 |

## 今回追加した実測と分類

- KAN8: 完成PR198 packageのHTTPcache内容5coldruns、実launcher conflict3guard。手で閉じたQAprofileをXDG-onlyへ移した入力fixtureであり、自動データ移行の合格ではない。既知length/prefixとrequest counterによる bounded content reuse、任意resourceのbitwise監査ではない。
- KAN48: 実VRTMAX site/manifest/iconを実製品で新規install/coldWayland/taskbar画像測定。原CachyOS/KDE6.7.5やVTMGO privacy flowは別条件。OSで既に信頼するmanagedproxy公開CAを専用profileへ通常NSS登録しTLS正常化。証明書検証無効化なし。
- KAN96: 通常/実container sourceのCOOP+COEP2UIinstall/2coldlaunch、実safe-mode制御fixture。actor/process/gate値は実測。source-containerをPWA container割当の合格にしない。原サイトMusicのproxy403も製品バグ/未再現にしない。
- KAN23: active signed-ja完全製品で標準collectorを実行しbootstrap不在timeout。さらに既存test-mode helperの生成設定sandbox1/CSPfalseを非起動で実測。ユーザーの保護維持条件に従い、そのrecipeはブラウザーへ適用しない。既存標準CIは開発harness成績、sandbox6完全製品試験とは独立。

## 資格とラベルの境界

正本: [Flathub198](https://github.com/flathub/one.ablaze.floorp/pull/198) / [3030](https://github.com/Floorp-Projects/Floorp/pull/3030) / [3032](https://github.com/Floorp-Projects/Floorp/pull/3032) / [依存3040](https://github.com/Floorp-Projects/Floorp/pull/3040)。

完成製品は既存production application/package工程と互換prebuilt native Runtimeによる未差替製品。fresh Gecko再コンパイル、strict helper/XPT/final-release、公式署名配布の合格ではない。Flathub198はlocal unsigned完全packageのみ。3030のen-US CIだけでは言語pack回帰を証明せず、ja/es-CL fullproduct4対照と生成manifestが別証拠。3032/3040はKAN48/89のverified original-icon fixとはしない。

env-blockedはLinux不在を意味せず、Bazzite/Bluefin/Manjaro/CachyOS等の未充足原条件として保持。既に解消した完成package/Plasma taskmodel/Wayland groupingの旧不足ラベルは再追加しない。今回満たしたcache/mixedguard/VRT/sourceCOOP条件は証拠範囲のpositiveラベルへ反映し、原OS/portal/permission/rollout/GPU条件を消さない。KAN49の初回rootlock原因は未解明のまま保持。

新たな製品欠陥は確認されず、推測修正/重複PRは作成しない。正本Draft198/3030/3032/3040は変更/mergeなし。Jira statusは維持しDone化なし。外部メッセージ/公開/リリースなし。再現script・ログ・画像は各KAN配下、元matrixはprevious-matrix.jsonへfingerprint付き保存。profile/HOME/runtime/生のprivacy URL/資格情報は公開対象外。

# native objdir 再利用の調査結果

現時点は **NO-GO**。host TU の増分 compile、xul link、実認証テスト、重い full build は実行していない。元原因は UNKNOWN のまま。以下は公開 source/run/artifact の照合結果であり、保存 objdir の内容を検証できたという意味ではない。

| 候補 | 公開 run / source | 保存意図 | provider ZIP size | 内容・依存の実証 |
| --- | --- | --- | ---: | --- |
| 12047907028 / x86_64 | 38885821823 attempt 1 / Runtime7dd | PGO generate の全 objdir | 5,470,281,993 bytes | 未検証 |
| 12044965771 / aarch64 | 同上 | PGO generate の全 objdir | 5,482,659,552 bytes | 未検証 |

固定 build-and-package.sh は PGO generate のみ全 objdir を tar に保存し、他モードは dist のみ保存する。元失敗 run38963901545 の非 PGO パッケージには、同 packager による全 objdir 保存はない。日次 workflow の条件と schedule event/job graph から、候補は intended debug=false / PGO generate、元実行は intended debug=true / non-PGO と推論できる。どちらも compiled mozinfo/Debug 状態を今回検証したものではない。構成を変えるための再 configure を短い増分処理に混ぜてはならない。

両 run の SDK 準備 job は Xcode26.6 / SDK26.5 を選択していた。ただし SDK artifact digest は異なり、SDK tree の同一性は未証明。候補は Linux から macOS への cross build で、Mac 上で profiler を実行できたことは、Mac で保存 objdir を再 compile できる証拠にならない。実 compiler/linker/plugin/Rust、全 flags、生成 header、absolute paths、mtime、link inputs、展開容量を確認できていない。

実 host は widget/cocoa/moz.build に非 unified / ARC / xul の一部として登録された MacWebAppService.mm。standalone floorp-app-shim はその host TU を含まない。通常の mach 部分 target でも configure/backend 再生成へ進み得るため、時間上限だけでは対象範囲を限定できない。make -n も build graph の読み取り専用性を証明しない。

再利用に進むための最小順序は次の通り。

1. 必要な archive の digest/CRC/安全な構造と、source/config/header/toolchain/SDK/dependency の全一致を確認する。config.status を調査目的で実行しない。backend から実 object target と argv、owner archive と全 xul 入力を確定する。
2. 条件が揃った場合だけ、Linux x86_64 cross 環境で実 host TU 1 件を固定 argv で compile する。提案上限は 120 秒。configure、backend 再生成、別 TU/Rust compile、依存取得や署名が必要なら停止する。
3. 検証済み owner archive/object list の更新と、app 外での xul link だけを別ゲートにする。link 提案上限は 600 秒、全 job は 20 分。これらは計画値であり、実 target/argv/resource guard の再レビューが必須。実 linker の自動署名も調べる。既存 app/package は変更しない。
4. 実認証には変更 xul を含む新 package と正しい署名由来が必要。既存 Darwin QA は host copy を ad-hoc 署名し、context-menu fixture も新 Shim.app を署名する。host 側の署名 step だけを外しても署名はなくならない。認証条件を緩めず、この経路は今回実行しない。

各段階は process group と子プロセス終了、実メモリ制限、実 df と archive/展開 objdir/source/SDK/toolchain/link 一時領域の合計予算を確認する。通常 wrapper、unrestricted mach build、timestamp 操作、欠損時の full build fallback は採用しない。厳密な条件は CONDITIONAL_EXECUTION_PLAN.json に記録した。

互換 objdir が使えない場合の最小 native 依存ビルド範囲は Intel 1 arch の configure/generated headers と、実 host TU・xul に必要な C++/Rust 全入力。ARM/universal integration、署名、公開、実認証 harness は含めない。元の x64 build job117185970899 は package を含め 172.05 分だった。計画上は約 150–210 分、210 分で停止する案であり、cache hit や所要時間を保証しない。TU compile だけに必要な configure/export 段階の時間は未測定。build 規則や linker が署名を実行する場合も停止する。将来の通常 native build に診断 patch を組み込む案では、PGO generate/use と既存公開 gate を維持する。

公開標準 runner の compute は GitHub の無料対象だが、artifact storage は別。超過分は現行 $0.25/GiB-month、実際の請求額・quota は確認していない。標準 Ubuntu の公開仕様は 4 CPU / 16 GB RAM / 14 GB SSD で、これを実空き容量や十分な build 容量とは扱わない。[GitHub runner 仕様](https://docs.github.com/en/actions/reference/runners/github-hosted-runners)、[料金](https://docs.github.com/en/billing/concepts/product-billing/github-actions)、[Mozilla macOS build 要件](https://firefox-source-docs.mozilla.org/setup/macos_build.html)を参照。

前回の小さい Foundation proof artifact12089125335 は今回 consumer 側で ZIP 実 bytes 1,248 を取得し、SHA-256、CRC、安全な単一 entry、内部 JSON と固定 job-log wrapper.report の一致を確認した。これは大きい objdir 候補の ZIP 検証とは別で、そちらの bytes/CRC は未検証。詳細は PROBE_ZIP_VERIFICATION.json に保存した。新しい模擬テストや追加 CI は行っていない。

固定 source の hashes と参照は SOURCE_INDEX.json、独立レビューは INDEPENDENT_REVIEW.json。公開証拠は code/run/artifact と検証条件に限定している。

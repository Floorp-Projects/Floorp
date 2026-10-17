# macOS Foundation 限定検証結果

[Runtime run 38999883111](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/38999883111) は attempt 1、単一の標準 `macos-15-intel` job 117300368731 で成功した。job は 2026-10-17 03:43:46–03:44:28 UTC（42 秒）、probe 内部処理は約 22.65 秒。

固定 fixture `cbe45a7d7480d713b2c40a9ed348420126e6ccb0` の 9 件の unfiltered unittest guards を必須 subprocess として実行し、成功後に Objective-C++/ARC をコンパイルした。コンパイル・native 実行の終了値はともに 0、native proof は 25 checks の完全一致で成功した。SDK 15.5、Apple clang 17.0.0、x86_64、macOS 15.7.9 / image 20260824.0482.1 を使用した。元 Runtime ビルドの SDK・完全な flags・deployment target との同一性は主張しない。

検証したのは、固定 Runtime7dd ソースへ診断 patch を適用した後の実 header、厳密抽出した RememberCause method と disconnect payload 式、および実 Protocol.mm codec である。first-cause/time/host-state 保持、NSString lifetime、NSNumber/JSON round trip、reset、codec 境界を確認した。base/patch/patched/header/method/payload/generated の各 SHA-256 は事前の独立静的導出と一致した。CoreFoundation の直接 link を明示する以外、凍結済み probe の動作を変更していない。

完全な MacWebAppService translation unit のコンパイル、xul の再リンク、実 host lifecycle・認証・watchdog の実行は行っていない。元の失敗原因は UNKNOWN のままであり、compiled Debug・製品・他 native target の資格証拠にはならない。診断 patch が既存配布 binary に組み込まれたことも意味しない。

証拠は同じ run/attempt/head の bounded job-log wrapper と結合した。artifact 12089125335 は 1,248 bytes、provider metadata digest と upload log digest が一致した。artifact は 1 日保存の JSON 単一 file である。consumer 側への ZIP materialization、ZIP 実 bytes の再 hash、CRC・展開内容の独立検証は未実施であり、job log から取得した proof と区別する。

既存の追加 browser 診断は [38994821629](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/38994821629) と [38996462140](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/38996462140) に保存済み（各 20/20 と GUI PASS）。それらの cleanup 後に観測された Shim exit 0 は元の失敗原因を説明する証拠ではない。今回、それらや旧失敗 run を再実行・置換していない。

Full rebuild は常に必須とは限らない。適合した configured objdir / generated headers / SDK / compiler / flags があれば host TU の個別 compile が可能で、host 実行には変更 object を含む xul の link と新しい test package が必要になる。既存 PGO archive は各約 5.5 GB の候補であり、取得も適合確認もしていない。詳細は BUILD_OPTIONS.json を参照。約 3 時間の Runtime full build は開始していない。

実行 caller は Runtime `d49872d2a8d70884d670fc9132728944baa4147b` に固定し、Floorp 側では CI patch と source fixture を維持する。既存 workflow・公開 gate・認証設定は変更していない。Floorp source branch の run は 0、Runtime probe branch の run は成功した今回の 1 件のみ。merge、署名、公証、製品公開、資格情報作成、本番設定変更は行っていない。

独立した API-only の結果レビューも同じ run/attempt/head、ソース・生成 hash、25 checks、artifact metadata と upload log を照合し、限定検証の PASS を受け入れた。未解決 P0/P1/P2 指摘は 0 件。詳細は INDEPENDENT_RESULT_REVIEW.json を参照。

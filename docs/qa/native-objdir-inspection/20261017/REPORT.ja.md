# 既存 Intel objdir の互換性検査結果

検査処理は完了したが、正式受入れと増分ビルドは **NO-GO**。完全 ZIP の実測 digest・entry CRC・gzip EOF/CRC は一致した。download 中の非推奨警告 3 件が事前の `downloadWarningsAllowed=false` を満たさないため、観測値は参考証拠として保存する。後から例外を追加せず、再実行も行っていない。

[Runtime run 39003241957](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/39003241957) / attempt 1 / head `ba20cf6707f56df6fd9b570241956576121e5c6d` / job `117310380175`。標準 `ubuntu-24.04` の 1 job、154 秒で SUCCESS。追加 Actions artifact は 0。Floorp 計画 branch の run は 0、両 default head は変わっていない。

対象は source run `38885821823` / attempt 1 / Runtime `7dd619742b25567e479eeda97ae071d10542ad0b` の artifact `12047907028` だけ。5,470,281,993-byte ZIP の実測 SHA-256 は `de764486eb0cbcbde256fcdce4b666408a6f4badfa1e9b5f21aad121976d911d` で固定値に一致した。ヘッダー確認は 367 bytes / 9 requests。内側 tar.gz は 5,522,162,304 bytes、SHA-256 `45cc947544eac908e7735d99f8be84b2f1d6239c4c940c4e10f967e972b19ae4`。

外側 ZIP の固定した単一ファイルを公式 Action で取得し、内側 tar は展開・実行せずに読んだ。64,982 members、非圧縮ストリーム 17,650,944,000 bytes、header inventory 8,456 entries を検査した。この読取量と今回の空き容量だけでは、将来の source・SDK・toolchain・リンク作業の容量を立証できない。

| 項目 | 保存資材からの観測 |
| --- | --- |
| debug / PGO | `mozinfo.debug=false`、`pgo=true`、`MOZ_PROFILE_GENERATE=true` |
| host / target | `x86_64-pc-linux-gnu` → `x86_64-apple-darwin` |
| compiler / Rust | clang `22.1.8`、Rust `1.95.0` |
| BuildID / version | `20261016101342` / `158.0.1` |
| host object | `widget/cocoa/MacWebAppService.o`、2,211,984 bytes、Mach-O x86_64 MH_OBJECT |
| dependency file | `.deps/MacWebAppService.o.pp`、550,098 bytes、mozilla-config.h 参照あり |
| XUL | 449,098,336 bytes、Mach-O x86_64 |

元の失敗 run `38963901545` は workflow 上 non-PGO / debug=true の意図だが、元 artifact の compiledDebug は未検証のままである。今回の PGO-generate を元の設定と同一視しない。debug symbols の有効化も Debug build の立証にはならない。

| 優先度 | 残る条件と判断 |
| --- | --- |
| P1 | 設定 cohort が異なる。元の non-PGO 検証には、実設定を観測した同 cohort の objdir または別途承認した新しい non-PGO build が必要。調査した packager は non-PGO 時に dist のみ保存する。 |
| P1 | archive 内 source revision、正確な SDK、完全な compile/link flags、LTO、外部 header の内容、link dependency closure が未立証。`source-repo.h` は 0 bytes、出力された INI 要約に SourceStamp はない。既知 archive/Rust path が未出力でも、他の path に存在しないとは断定しない。 |
| P2 | download 依存の DEP0040 / DEP0169 / DEP0005 と、Node20 action を Node24 で強制実行した runner 警告 1 件がある。正式受入れは NO-GO。将来の取得前に対応 pin と extraction/digest 実装を独立確認する。 |

P0 は検出していない。新しい reader の P1/P2 は実行前に修正・再レビュー済み。上記は実物観測後の受入れ・再利用条件である。digest mismatch、HTTP 401/403、retry error はログに見えていない。内部 retry が 0 回だったことまでは証明していない。

full host translation unit の再コンパイル、XUL link、認証試験、署名、製品 / Debug qualification は未実行。元の失敗原因は UNKNOWN。ARM payload は取得していない。使い捨て隔離コピーの ad-hoc 署名は今後の承認済み試験で利用でき、今回の NO-GO 理由には含めない。

次段階では親が cohort と実行範囲を選び、source + native patch、toolchain / SDK、flags・依存 closure・資源上限を結び付けた計画を確認する。PGO の追加 TU 検査も元 non-PGO の qualification を代替しない。長時間 full build と認証試験は今回開始していない。

詳細は `JOB_PROOF.json`、`TRANSPORT_GATE.json`、`COMPATIBILITY_ASSESSMENT.json`、`INDEPENDENT_RESULT_REVIEW.json`。事前条件は `ACCEPTANCE_CRITERIA.json`、固定呼び出し側は `IMMUTABLE_CALLER.json`、実行後照合は `POST_RUN_INVARIANTS.json` に残した。

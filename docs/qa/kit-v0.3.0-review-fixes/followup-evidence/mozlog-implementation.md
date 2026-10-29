# QA309 mozlog: P2-NEW-02 / P2-NEW-03 の修正

2026-10-09。担当 `/root/review_release_contracts`。Runtime base head `460106115bc76b026c8bf8dad24df7fcb07f97c2`。変更した repository source は `qa3_mozlog.py` と `test_qa3_mozlog.py` の2ファイルだけ。commit/push は主担当へ委ね、実施していない。

## 変更

[qa3_mozlog.py](/workspace/qa-review-v030/repos/Floorp-Runtime/.github/workflows/scripts/qa3_mozlog.py) の `verify_mozlog` は `test_status` を assertion に加算する前に `subtest` の存在を検査する。JSON null または文字列を許し、欠落、raw bool/int/float/list/object を拒否する。正式 Mozilla writer が出力できる空文字・空白・Unicode・null は保持する。canary の別条件である正確な理由・phase・ID・整数 exit 1・hit数、required skip/first FAIL/unknown/child-exit 拒否は維持した。別の古い kit の非空名 policy は変更していない。

`MAX_EVENT_BYTES = 1024**2` を導入し、`stream.readline(MAX_EVENT_BYTES + 1)` によって過大な行を全部読み込まずに拒否する。元の 1 MiB は改行を含む bytes 上限のまま。LF が無い final event は完全な JSON であっても INCOMPLETE な raw frame として拒否する。LF/CRLF、複数の完全イベント、正常な UTF-8 は保持し、partial JSON と壊れた multibyte UTF-8 は成功にならない。2 GiB raw / 1,000,000 events の上限は変更していない。

## RED → GREEN と既存テスト保持

専用 [test_qa3_mozlog.py](/workspace/qa-review-v030/repos/Floorp-Runtime/.github/workflows/scripts/test_qa3_mozlog.py) は旧14 methods に7 methodsを追加した。小さい mocked cap 256 bytes を使い、巨量/OOM 実験は行っていない。

| 検証 | 結果 | 生証拠 |
|---|---|---|
| 本体修正前 RED | 21 methods / 13 failures（subTest含む）/ exit 1、timeout false。parser SHA は固定 base と一致。 | [mozlog-red-01.log](/workspace/qa-review-v030/fix-qa309/evidence/mozlog-red-01.log) |
| 本体修正直後 GREEN | 21 methods / exit 0 / OK | [mozlog-green-01.log](/workspace/qa-review-v030/fix-qa309/evidence/mozlog-green-01.log) |
| 最終 format/互換修正後 GREEN | 21 methods / 0.039 seconds / exit 0 / skip 0 / OK | [mozlog-green-02.log](/workspace/qa-review-v030/fix-qa309/evidence/mozlog-green-02.log) |
| Ruff check | exit 0、All checks passed | [mozlog-ruff-check-green-01.log](/workspace/qa-review-v030/fix-qa309/evidence/mozlog-ruff-check-green-01.log) |
| Ruff format check | exit 0、2 files already formatted | [mozlog-ruff-format-green-01.log](/workspace/qa-review-v030/fix-qa309/evidence/mozlog-ruff-format-green-01.log) |
| 対象2ファイルのみ `mach lint --linter ruff --linter ruff-format` | exit 0、0 problems / 0 errors / 0 warnings | [mozlog-mach-lint-01.log](/workspace/qa-review-v030/fix-qa309/evidence/mozlog-mach-lint-01.log) |
| scoped `git diff --check` | exit 0 | [mozlog-diff-check-01.log](/workspace/qa-review-v030/fix-qa309/evidence/mozlog-diff-check-01.log) |

新規 methods は、必須 field/raw 型、許容 null/文字列、readline が要求/返却する bytes の上限、cap−1/cap/cap+1、未終端完全/partial event、Unicode の bytes 境界と壊れた UTF-8、CRLF を検証する。bounded-reader fixture は 257 bytes しか読み込まずに拒否することを観測する。multibyte の bytes 長と文字数の違いも small cap で検査した。

最初の Ruff は新テストの括弧付き `with` が repository の Python3.8 target と不一致で失敗した。初回失敗は [mozlog-ruff-check-red-01.log](/workspace/qa-review-v030/fix-qa309/evidence/mozlog-ruff-check-red-01.log) と [mozlog-ruff-format-red-01.log](/workspace/qa-review-v030/fix-qa309/evidence/mozlog-ruff-format-red-01.log) に保持し、互換構文と formatter で修正した。本体の guard/readline の意味はその後変更していない。

旧14 test methods はすべて AST が初期コピーと同一で、削除・期待値緩和は無い。[最終確認](/workspace/qa-review-v030/fix-qa309/evidence/mozlog-final-verification.json)、[scoped patch](/workspace/qa-review-v030/fix-qa309/evidence/mozlog-fix.patch)。全 Runtime suite は主担当が同時進行する他の修正を合わせて検証する範囲であり、ここで21件を全Runtime成功と表示しない。

## 再現条件・凍結 byte・制約

fixture の外側は allowlist env、独立 process group、AS 512 MiB / CPU 15 s / FSIZE 8 MiB / wall 30 s。lint は AS 1–2 GiB / CPU 15–30 s / FSIZE 8 MiB / wall 30–60 s。pip/uv offline、既存 cached tool を使い、ネット要求はしていない。各 run の argv・上限・時間・source SHA は同名 `.json` に保存し、証拠は排他的作成で上書きしていない。

* `qa3_mozlog.py`: SHA256 `670a7ea7cf69db66801ec98d1932136ae8dbe6f1b2ae0bed34063a437eb078ff`、7,839 bytes。
* `test_qa3_mozlog.py`: SHA256 `5625812d4eab52274efa040100155fa376dfe21672bedfc67dc033d3f4e0e063`、14,148 bytes。
* scoped patch: SHA256 `826aa5ad8413c29f4d12382be185b777f369eb9a00e9fac4021e1b2e4d0e0c41`。

AGENTS と [mozlint skill](/workspace/qa-review-v030/repos/Floorp-Runtime/.agents/skills/mozlint/SKILL.md) を読んで適用した。workflow・I/O・setup は他担当の所有で触れていない。native/build/model/browser/private runner/paid run/mach try/dispatch/資格情報/公開操作は無い。既存の通常 public PR CI の autostart を禁止・取消する変更も無い。

この2指摘の修正と小型回帰固定化は完了し、独立 post-fix review に渡せる。資格認定・全run履歴・最終製品 artifact 接続は今回変更せず、native/product qualification 未実証のまま。実 proof の許可をこの unit 成功から導かない。

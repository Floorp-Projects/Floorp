# Mac上流試験の詳細ログ回収候補

2026-10-29 JST、Floorp 開発 B。M1の失敗調査を進めるための候補で、Mac42件や製品全体の合格を意味しない。

前回のIntel run40562022078では、公開射影だけが残り、scoped harnessの完全ログを回収できなかった。
今回の候補は公開用の固定語彙による診断を保持し、詳細ログの限定したsnapshotを公開鍵で暗号化して保存する。
Runtimeの候補は引き続き `docs/qa/mac-native-firefox45-diagnostics-20261029.yml.disabled` に置き、実行済みworkflowは変更しない。

## 保存する範囲

`diagnostic_archive.py` が読むのは、専用task内の次の8パスだけである。

- `job-origin.json`
- `summary.json`
- `first-failure.json`
- `pipeline.private.log`
- `mozilla-upstream-xpcom/receipt.json`
- `mozilla-upstream-xpcom/first-failure.json`
- `mozilla-upstream-xpcom/native-private/harness.raw.jsonl`
- `mozilla-upstream-xpcom/mozilla-xpcshell.private.log`

SDK、OBJDIR、任意の追加ファイル、環境変数は列挙・収集しない。各パスの上限を固定し、既存のFD readerで合計64 MiB・30秒のsnapshot上限、所有者、regular file、単一hardlink、全祖先とFDの前後同一性を検証する。symlink、FIFO、サイズ超過、欠落はその理由を記録し、短縮したログを完全なログとして扱わない。

元のbytesと限定manifestから作るtarはメモリ内だけに置く。OpenSSL 3のCMS AuthEnvelopedData、AES-256-GCM、RSA-OAEP/SHA-256で暗号化し、ciphertextだけをartifactに指定する。公開証明書のSHA256をsourceとworkflowに固定する。復号鍵は担当Bの私有領域に保存し、repository、回帰sandbox、共有成果物、GitHubへ渡さない。暗号化artifactの保持期間は14日とする。

公開receiptの `scopedFilesCaptured` は2つのscopedファイルをsnapshotできたことだけを表す。内容の終端性、test合否、native認定、製品認定は付与しない。復号時はOpenSSLの終了0とGCM認証を確認してから内容を利用し、manifest内のrun/head/attempt/job、各ファイルhashを同じ実行の保存証拠と再照合する。

暗号方式の仕様は [OpenSSL CMS](https://docs.openssl.org/3.5/man1/openssl-cms/) と [AuthEnvelopedData導入記録](https://github.com/openssl/openssl/blob/master/CHANGES.md) を参照。

## 起動・失敗時の扱い

新しい明示selector `--diagnostics-20261029` は `b/qa-fresh45-diagnostics-20261029` の新規push・attempt1だけを認める。認証済みAPIのrunとjobも同じbranch/headでなければ拒否する。旧branch・rerun・dispatchを受け付けず、job起点の345分work／360分jobと時計の再初期化禁止を維持する。

候補workflowでは、origin初期化後、SDK取得・build前に暗号化transportをprobeする。公開診断artifactはverifier失敗時も保存する。暗号化はoriginとprobeの成功を条件に `always()` で行い、成功した暗号化stepのciphertextだけを別artifactへ保存する。保存処理を成功させるためにpipeline/verifierの失敗を許容状態へ変更しない。

実行済みのquery 3秒／3 samples、profile 30秒、upstream test/assertion/manifest/plan、source/FD identity、owned cleanupは変更しない。

## 検証と残る条件

新しい13ケースは、実際の使い捨て鍵による暗号化・復号、改ざん拒否、SDK・鍵・環境の非収集、FD差替え・symlink・hardlink・FIFO・サイズ超過・欠落・宛先上書きの扱いを検証する。新originの4ケースは旧routeやrunの混用を拒否する。

Linux隔離での回帰と公開verifierのsynthetic controlは、Macにおけるtransportや42件の実行証明ではない。
native開始前に、Mac runner上のOpenSSL 3の実在・version・由来、最新の公式SDK metadataと取得bytes、最終fixture/head、全source pins/catalog、branch不存在とpush run0を凍結する。SDKは `2026-10-29T13:57:37Z` という取得済みmetadataの期限を越えて有効扱いしない。Mac用toolchainの依存を先に確かめ、同条件の盲目的な再実行は行わない。

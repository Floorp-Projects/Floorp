# コンテナ対応ワークスペース同期 QA

検証日: 2026-10-28（日本時間）

PR: [Floorp #2999](https://github.com/Floorp-Projects/Floorp/pull/2999)

製品コードは `94058e4e19c3a266b631a058c4a59d5f1fbee948` を対象とする。 Runtime
は `0ac255739cdd64e100cd6913329247421b0ec2ea`（Gecko 159）。 完全な Linux Floorp
12.21.0 アプリに通常の production build と Runtime patch を適用し、 生成された
882 リソースを照合した。通常利用のプロフィールとは独立した環境で検証した。

## 自動検証

| 検証               | 結果           | 内容                                                                              |
| ------------------ | -------------- | --------------------------------------------------------------------------------- |
| Production build   | 成功           | Chrome、ESM、各ページの通常ビルドとパッケージ内の出力照合                         |
| Smoke              | 成功           | 6 ステップ。型検査・lint・テスト検出など                                          |
| Host tests         | 367 件成功     | Runtime patch の適用・開発版との対応、関連ツールの回帰                            |
| Browser model      | 5 スイート成功 | 文書、レコード、コンテナ参照、関連付け、移行                                      |
| Native engine      | 39 件成功      | v2、削除保護、通常 close、明示解決、移行中編集、永続再送、アカウント切替          |
| Native tab adapter | 25 件成功      | タブ・SessionStore・navigation・失敗再開の既存回帰                                |
| Chrome UI          | 33 件成功      | 設定、再関連付け、明示移動、起動待ち、private window、複数ウィンドウ              |
| Native migration   | 17 件成功      | 9 native ケースと 8 model ケース。暗号化、ページ取得、CAS、所有権、再開、保存失敗 |
| QA tools           | 15 件成功      | Linux 起動、CA・proxy 設定、patch 適用、プロフィールを維持するアプリ更新          |

Native 統合は最終製品コードで再実行した。Chrome UI は
`f966f4147956538b6388919b401a892324644e77` の production 成果物で確認し、
その後の製品差分は Sync engine の送信候補整理だけである。

削除 lifecycle の先行検証 6 件に加え、実際の beforeunload キャンセル 2 件、
無効化要求が完了処理と重なる競合、外部 URL の hot/cold/new-window 8
ケースも確認した。 再送フラグについては永続ファイルの再読込・空の tracker・部分
ACK を検証している。 OS の電源断を発生させた試験ではない。

主なローカル証跡:

- Native 統合: `_dist/workspace-sync-manual/run-1508f7c58a0896e4/results.json`
- Chrome:
  `_dist/container-sync-qa/chrome-native-vite-9a4356ce1723b877/results.json`
- 移行: `_dist/workspace-migration-native/run-cd6fa9bef1627875/results.json`
- 外部 URL:
  `_dist/container-sync-qa/native-external-3a2db455c93a9d95/results.json`
- beforeunload:
  `_dist/container-sync-qa/native-beforeunload-2241a3bd46afbc56/results.json`
- 無効化競合:
  `_dist/container-sync-qa/native-disable-race-32b44ea53b33acbf/results.json`

## 実アカウントでの二クライアント検証

独立した二つのアプリ・プロフィールで、同じ専用アカウントの認証と Sync
準備完了を確認した。 実 Mozilla Sync の HTTPS 通信で、以下の 14
項目すべてが成功した。

1. 選択したワークスペースとタブの転送、ローカル専用の除外。
2. 双方向の名称・URL・タイトル・pin 変更。
3. 両側の未送信追加の収束。
4. 古い並び順・挿入に対する削除通知の優先。
5. 空ワークスペースの同期。
6. ローカル同期解除と明示的な再選択。
7. 同名コンテナの既存セッション再利用と、明示的 none の分離。
8. 異なるローカル ID の同名確認、ローカル情報・名称変更の保持。
9. ネイティブ UI での明示再オープンと、同期タブ UUID の保持。
10. 新規コンテナと none に送信元の Cookie が転送されないこと。
11. 一致なしの自動作成、ワークスペース既定の継承、明示的 none の保持。
12. コンテナ削除による閉鎖の非転送と、明示復旧での同じ UUID の再展開。
13. テスト用ワークスペース削除の同期と、受信側ローカルタブの保持。
14. 他の同期エンジンの metadata・declined 設定の fingerprint 不変。

証跡:
`_dist/container-sync-qa/live-01/acceptance-6451abf6-1e0d-4936-8a5f-0e25cfb1109c.json`。
製品コードの revision と 882 リソースのハッシュを同じ report に記録した。
テスト用のワークスペース・タブの同期レコードは通常の削除経路で片付けた。
コンテナ定義、ローカルコンテナとサイトデータは製品仕様に従って保持する。

認証情報・認証コード・実サービスの Cookie は文書や結果レポートに含めない。
Cookie の検証は localhost の試験用 HTTP
サーバーが発行する値だけを扱い、結果を真偽値で記録する。

## 受入中に見つけた問題の修正

- 関連付け変更時に、既存タブの所属が希望状態を上書きしないよう保留を先に保存する。
  明示的なタブ移動は通常の削除・追加として反映し、削除保護による再出現を防ぐ。
- 親ワークスペースの削除受信後、参照がなくなったコンテナ定義を送信候補から外す。
  他の選択中ワークスペースの共有定義、移行 manifest、正当な削除通知は保持する。
  ローカルの関連付けやサーバー上の定義は削除しない。 修正前は実 Sync
  で最初の同期が失敗し、再同期が成功することを再現した。
  修正後は同じ手順の初回から同期成功、ローカルタブ保持を確認した。

削除競合の修正前後の証跡:

- 修正前:
  `_dist/container-sync-qa/definition-delete-repro-53e95b10-9023-40cd-874f-709941b64e0a.json`
- 修正後:
  `_dist/container-sync-qa/definition-delete-repro-a3a54005-c8eb-4a8d-87b6-1559d3791395.json`

## 再実行と検証範囲

実行方法は [QA ツールの README](../../../tools/workspace-sync/README.md)
を参照。 アプリを更新して再実行する場合は、専用アプリを終了してから
`refresh-acceptance-apps.py` を使い、元の認証済みプロフィールを維持する。

今回の実機受入対象は Linux 上の独立した二クライアントである。 物理的な異 OS
間の操作、iOS の受信、配布用インストーラーのリリース検証は対象に含めない。
移行所有元のジャーナル紛失時は同期を停止する。 所有権移管と旧 v1
の未送信編集の手動回収 UI は今回実装していない。

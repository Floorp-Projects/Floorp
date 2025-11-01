# main ブランチ保護ポリシー（直接プッシュをブロックするための手順）

このドキュメントは、`main` ブランチへの直接プッシュをブロックし、必ず PR 経由でマージする運用にするための手順と推奨設定をまとめたものです。

注意: GitHub のブランチ保護設定はリポジトリの管理画面（または GitHub API）から有効化する必要があります。本ドキュメントは設定手順と、必要なレポジトリ内の雛形ファイルを提供します。

## 運用方針（要点）

- 直接 push を禁止し、すべての変更は PR を通すこと
- PR は必須の CI チェック（`ci/pr-check` 等）をパスすること
- CODEOWNERS によるレビュワー承認（少なくとも 1 人）を要求すること
- 必要であれば "管理者も保護ルールの対象" に設定して、管理者による強制マージを禁止する

## 管理画面での推奨ブランチ保護設定

1. ブランチ名: `main`
2. 「Require pull request reviews before merging」を有効化
   - Required approving reviews: 1（チームポリシーに応じて 2 にしてもよい）
   - 「Include administrators」を ON にする（管理者もルールに従わせる）
3. 「Require status checks to pass before merging」を有効化
   - 必須チェックに `pr-check`（ワークフロー名に依存）等、CI ワークフローを追加する
4. 「Restrict who can push to matching branches」を有効化（オプション）
   - もし必要なら、特定のリリースチームまたは自動化アカウントのみを許可する

## リポジトリ内で追加した雛形

- `.github/CODEOWNERS` — コードオーナーを指定して自動レビューを要求するテンプレ
- `.github/workflows/pr-required.yml` — PR 上で走らせる CI ワークフローのテンプレ（実運用のテスト/ビルドコマンドに置き換えてください）

## 運用手順（短い手順）

1. 管理者が GitHub のリポジトリ Settings → Branches で `main` の保護ルールを追加
2. 上記で「Require status checks」を有効にする場合、まず `.github/workflows/pr-required.yml` を用意して PR チェックを実行できるようにする
3. `.github/CODEOWNERS` を作成し、適切なチーム／レビュワーを指定
4. チームに新しいルールを通知し、直接 push がブロックされることを周知する

## 例外と Hotfix の扱い

- 緊急の Hotfix が必要な場合は、専用の `hotfix/<id>` ブランチを作り、PR で main にマージする。管理上どうしても直接 push が必要なケースは運用上の例外として手順を定め（管理者承認・監査ログ）、ただし原則は禁止。

---

このファイルは運用テンプレートです。組織のポリシーに応じて数値（必要なレビュー数など）を調整してください。

作成日: 2025-10-29

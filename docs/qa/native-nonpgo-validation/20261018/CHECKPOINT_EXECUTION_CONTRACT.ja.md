これは直列の標準 `macos-15-intel` 2 jobs を1回だけ実行する、固定条件の再開ビルド契約です。以前の `CHECKPOINT_LOCAL_DESIGN.ja.md` と `CHECKPOINT_LOCAL_RESULT.json` は中断方式も検討したローカル実験の記録です。今回の実装は任意の中断状態を保存対象にしません。

producer は空の objdir から configure、`mach build -j 3 pre-export export`、生成された root graph の検査、`mach build -j 3 -C . toolkit/library/rust/target-objects` を順に実行します。全 stage の exit 0、所有 group 消滅、leader reap、監視成功、準備前からの worker UID/RUID census に対する新規残存 identity なしを要求します。生成された Rust profile、archive、depinfo を確認し、host Cocoa object と XUL は未生成であることも確認します。240分を過ぎれば通常の失敗として安全に回収し、その状態を保存しません。

自然成功後に source/tools/SDK の内容、obj/Cargo/venv のファイル内容・mode・mtimeNS・typed symlink を manifest に記録します。Cargo の hidden fingerprint、build-script output など、再開の意味に必要な生成メタデータは改変せず保存します。これらは固定の公開 source と token を渡さない offline build 環境で生成されます。QA の `*.private.log`、Mach の診断ログ、過去の QA timer/PID/native/profile 状態は保存しません。mozbuild state は当該 source の venv と topsrcdir binding のみに限定します。閉じた compiler log は先頭から EOF まで走査し、bytes/hash、分類件数、安全化した先頭/直近診断を記録します。raw compiler log、環境全体、token、署名付き URL は公開証拠へ含めません。

artifact は gzip tar の単一 raw file だけです。SHA 固定 `actions/upload-artifact@cf430e030ddbb5b0abf93d22962f4752f3646cd9` の `archive: false` を使用し、ZIP wrapper を追加しません。書込み中から最終格納 bytes 10 GiB を上限とし、超過時は upload 前に失敗します。logical bytes 30 GiB、state entries 200000 は試行上限であり実測値ではありません。完全な読戻し検査、digest、サイズ、残 disk、残時間を upload 前に確認します。upload は1回、overwrite/cache なし、`retention-days: 1`、service metadata の ID/digest/サイズ/期限/当該 run の artifact 数1を確認します。

producer の期限は job 開始から自然成功240分、manifest走査260分、pack/hash/読戻し305分、upload335分、service照合と終了345分です。各 job の hard limit は360分です。既存の RSS 10240 MiB、build 前40 GiB、継続中8 GiB reserve、log cap、複数信号による stall 検出、監視取得不能の停止、identity を確認する cleanup を維持します。

consumer は新しい job 原点と QA state を作成し、同一 pin の source/tools/SDK を同じ absolute root へ用意します。API で producer job の成功と artifact 来歴を確認し、取得 bytes と digest を照合します。空の staging に regular payload だけを手動展開し、path traversal、duplicate、special file、不適切な mode、未知の link を拒否します。source/tools/SDK の内容照合後にのみ typed link と producer mtimeNS を復元します。実際の `mach environment --format json --verbose` を再取得し、producer の effective configuration と比較します。manifest の自己申告で代用しません。

consumer は restore/setup60分、通常の full build300分、package315分、native suite335分、結果と cleanup345分を期限とします。full host object/XUL、package、disposable copy の ad-hoc 検査、native authentication/cause suite、post-test content binding をすべて要求します。host object は producer に存在せず、consumer 開始後の生成時刻を要求します。job3 や追加 checkpoint は作成しません。

成功時も `verified-resumed-build-across-two-standard-intel-jobs`、`freshObjdir=false`、`sameJob=false` と記録します。product qualification は false のままです。自然境界の実 Mac 完了時間、サイズ、別 VM での完全な復元、2 jobs 内の full build/native 完了は今回の実測で判定し、事前に成功とは呼びません。

SDK transport pin は正規 run 39066132056 / artifact 12113511845（失効2026-10-18T20:19:27Z）です。未失効時に正規 API 経由で取得し、600秒取得期限・HTTP error 停止・ZIP/TAR hash・canonical tree・version 26.5 を維持します。ローカル照合で旧 pin と canonical tree `c3c01387c82e9ce1436fd517ec19308416663cc526c39be15f66f6bab22c3a5f` が一致しました。SDK を checkpoint に格納せず、consumer も未失効時に正規取得します。取得済み SDK の利用と build の6時間を結合しません。

根拠は固定 Runtime の [recurse.mk](https://github.com/Floorp-Projects/Floorp-Runtime/blob/7dd619742b25567e479eeda97ae071d10542ad0b/config/recurse.mk)、[rust.mk](https://github.com/Floorp-Projects/Floorp-Runtime/blob/7dd619742b25567e479eeda97ae071d10542ad0b/config/makefiles/rust.mk)、[backend generator](https://github.com/Floorp-Projects/Floorp-Runtime/blob/7dd619742b25567e479eeda97ae071d10542ad0b/python/mozbuild/mozbuild/backend/recursivemake.py)、[upload action input contract](https://github.com/actions/upload-artifact/blob/cf430e030ddbb5b0abf93d22962f4752f3646cd9/action.yml) です。Cargo build-script OUT_DIR は中断後の自動無効化を保証しないため、任意中断の復元へ一般化しません。

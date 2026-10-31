# Mac実行入力の事前検証

2026-10-29 JST、Floorp 開発 B。QA #2513ロードマップM1の継続。

長時間の `fresh-firefox45` を始める前に、暗号化診断が必要とするMac側の依存と、期限が近いSDKを確認する。
この事前検証はMac42件や最終製品の認定ではない。

## 今回確認したこと

- Floorp `94295a47715b8be669d809d2678225e2f13b85d8` の通常CI
  [run40770943029 / attempt1](https://github.com/Floorp-Projects/Floorp/actions/runs/40770943029)
  は終端successだった。smokeとLinux x64、Windows、Mac、Linux ARMの4環境jobを照合した。
- SDK artifact `12739697223` を正規API経路から再取得し、ZIPの105082683 bytesと
  SHA256 `2f8aa854ddae53d2edb6fa300e0583cb823a4ef20c419a810fbf001d414c8c23` がAPIのsize/digestと一致した。
  前後のmetadata、producer run `40516581212` / attempt1 / job `122231898010`、producer commit
  `a2bceca6e076c130cb133e490e0f7c5f50f34c30`、未失効を照合した。
- ネットワーク・秘密情報なしの隔離環境で全内容を再検証した。uncompressed tarは760518144 bytes、
  SHA256 `57aeb200890ba97c382bf450f23f9dab3e0d4696fdf02323a705f82210b087e3`、
  treeは49701 members / 704719765 logical bytes / canonical SHA256
  `c3c01387c82e9ce1436fd517ec19308416663cc526c39be15f66f6bab22c3a5f`、SDK versionは26.5だった。
- 旧SDKの期限は `2026-10-29T13:57:37Z`（同日22:57 JST）。次の6時間周期では期限を越えるため、
  同じ内容を新しいproducerで取得する。期限延長や旧metadataの流用は行わない。

Mac Intelの旧実行ログが示すimageは `macos-15/20260824.0482`、version `20260824.0482.1`。
確認時点の[公式runner image資料](https://github.com/actions/runner-images/blob/d53fb259895d2a1207bcdd30f288557f01288b77/images/macos/macos-15-Readme.md)
ではPython 3.12.10のcacheと既定のOpenSSL 1.1.1wを確認できた。
[image構築処理](https://github.com/actions/runner-images/blob/d53fb259895d2a1207bcdd30f288557f01288b77/images/macos/scripts/build/install-openssl.sh)
もmacOS15では1.1を既定にする。これだけでは候補の `/usr/local/opt/openssl@3/bin/openssl` の実在・version・来歴は未証明なので、短時間の実機確認を先に行う。

## 限定した実機事前検証

Runtimeの新しい `.github/workflows/qa-native-input-probe-20261029.yml` だけを追加する。
実行済みworkflowと `mac-native-firefox45-diagnostics-20261029.yml.disabled` は変更しない。

起動は自分の既存 `b/qa-handoff-20261026` への、parent
`11f66dbfa00471e3c6a13993888c4c85cb1ccaf8` からの今回のpushだけ。
workflowの変更パス、repository、ref、push、created=false、before、attempt1を固定する。
rerunや別ref・別parentを拒否し、同じ条件を2jobで検証する。

1. `input-provenance`：`macos-15-intel`、最大10分。immutable fixture
   `94295a47715b8be669d809d2678225e2f13b85d8` と148 source hashesを照合する。
   実Python 3.12.10、CPU/OS/image、OpenSSL 3の選択・解決先、binary・2つのlibrary・Homebrew installation receiptのhashとversionを記録する。
   既存の暗号化回収13 controlsをMacで実行し、全件成功・skipなし・一時鍵と一時ファイルのcleanupを要求する。
   残すのは固定形式のpublic proofだけで、raw log・鍵・環境変数はuploadしない。
2. `download-macos-sdk`：前jobの終端後、`macos-26`、最大20分。
   Xcode26.6 / build17F113 / SDK26.5と、元の完全なuncompressed tarのsize/hashを要求する。
   依存確認が失敗した場合も、同じpush条件を満たす場合に限りSDKの確認を進める。
   SDK artifactは7日保持し、上書きしない。将来のnative入力への採用は、実際の新artifact ID、
   repo/run/head/attempt/job、metadata/expiry/digest/size、正規取得bytesと完全内容の再照合後に行う。

2jobは直列で、合計のjob上限は30分。package install、Firefox build、42件のharness実行は含めない。
SDK producer commitと、コンパイル元Runtime `7dd619742b25567e479eeda97ae071d10542ad0b` は区別する。

## 検証と次の条件

今回QA helperのsourceは変更していない。既存の754-ID catalogと148 hashesを再照合し、
変更なしの749成功・実Darwin専用5skipという前回回帰を再実行・加算しない。
新workflowのshell/Python構文、狭い起動条件、直列順序、時間上限、固定uploadパス、
SDK同一性条件と既存producerの不変性を隔離環境で確認する。
正しいpushと6つの誤った起動条件、LinuxからMac proofを作ろうとした場合の拒否を検証する。

事前検証の終端後に同run/head/attempt/jobとproof・SDK artifactを照合する。
OpenSSLが不在・非対応、controls失敗、SDK内容不一致の場合はその実結果を保持し、
必要な変更条件を確定するまでfresh45を始めない。
成立後に新SDKの明示pin、image/toolchain、fixture、全source hashesとcatalog、
originとclockを凍結し、`b/qa-fresh45-diagnostics-20261029` の不存在・push run0・未送信を再照合して初回1回へ進む。

旧Intel run40521422626・40562022078の消費済み失敗と、42 UNVERIFIED + 3 MANIFEST_DISABLEDは維持する。
今回のSDK内容確認、通常CI成功、将来のdependency proofをMac42件や製品全体の合格に換算しない。

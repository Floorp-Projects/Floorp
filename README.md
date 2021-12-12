# Firefox ベースの Floorp のビルド方法

## 前提条件
----

### OS オペレーティングシステム ハードウェア要件

・Windows11 or Windows 10

・Ubuntu (20.04 推奨)

・macOS X


メモリ： 最小4GB RAM、8GB以上を推奨。

ディスク ： 容量 少なくとも40GBの空きディスク容量。

----
### Windows でのセットアップ

・Visual Studio 2019 or Visual Studio 2022 ビルドツール
##### 要求されるコンポーネント Visual Studio 2019
 ` C ++を使用したデスクトップ開発。`

 ` C ++によるゲーム開発。`
 
` Windows 10 SDK（最新版）`

`v142ビルドツール（x86およびx64）用のC ++ ATL`

##### 要求されるコンポーネント Visual Studio 2022 ビルドツール

` C ++を使用したデスクトップ開発`

 ` Windows 10 SDK（少なくとも 10.0.19041.0 ）`
 
`  v143ビルドツール（x86、x64）用のC ++ ATL`


・Mozilla Build

Windows 向けのビルドツールをダウンロードする

https://ftp.mozilla.org/pub/mozilla.org/mozilla/libraries/win32/MozillaBuildSetup-Latest.exe

C:\mozilla-build\start-shell.bat からビルド用のターミナルを起動する。Linuxの最低限のコマンドが実装されている(rmなど)。終わったら、ソースコードのセットアップへ。

-------
### Windows でのソースコードのセットアップ

Firefox のサーバからソースコードをダウンロードする C: を使っているがどこでもいい。後、ターミナルは絶対mozbuildダーミナル

``` 
cd c:/
mkdir floorp
cd floorp
git clone https://github.com/Floorp-Projects/Floorp-legacy-dev.git

完了後

cd floorp-legacy-dev

mach bootstrap
```
*注意

`bootstrap実行時、1~4の番号選択がありますが、2番の「アーティファクトビルドをしない」を選択してください。それ以外では正しくビルド出来ません。`

------
### Linux でのセットアップ

・ Mercurial

Firefox はソースコードを hg で管理する為、Mercurial がダウンロードに必要。また、次のコマンドには Python が必要。

```
sudo ap install mercurial  && sudo apt install git && sudo apt install python3 && sudo apt install pip
```
 「hg version」 で存在を確認できる。

以下のコマンドで  FLoorp のソースコードをダウンロード出来る。

```
mkdir floorp
cd floorp
git clone https://github.com/Floorp-Projects/Floorp-legacy-dev.git

完了後

cd floorp-legacy-dev

mach bootstrap

```
*注意

`bootstrap実行時、1~4の番号選択がありますが、2番の「アーティファクトビルドをしない」を選択してください。それ以外では正しくビルド出来ません。`

終わったら、Floorp のソースコードを統合するに進んでください。

----

### Floorp のビルド

ビルド
```
cd c:/floorp/mozilla-beta/     //ソースコードのディレクトリに入る//
./mach build   　　　　　　　　　　　　　　// ビルドには約100分かかります。//
```
ビルド完了後 Floorp を実行する
```
//ソースコードのディレクトリに入る//
./mach run                                //Floorp を実行//
```
```
//インストーラー化//
./mach package
```

Windows であれば、セットアップ用の exe が `"C:\mozilla-source\mozilla-unified\obj-x86_64-pc-mingw32\dist\install\sea\firefox-95.0a1.en-us.win64.installer.exe"` に

Linux であれば、tar .bz ファイルが `..\mozilla-unified\obj-x86_64-pc-gnu-linux\dist\install\sea\firefox-95.0a1.en-us.win64.tar.bz`に生成される。

----

## ビルド時に上手く行かなかったら

issue は建ててもいいですが、開発者は対応しないかもしれません（忙しいため）

その前に試してほしいことは以下の通りです。

・コマンドプロンプトの再起動

・./mach clobberの実行(ビルドファイルのクリーンアップ)

・変更した部分の書き戻し

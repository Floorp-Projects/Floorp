/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//import utils
ChromeUtils.import("resource://gre/modules/osfile.jsm");
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function getSystemLocale() {
    try {
      return Cc["@mozilla.org/intl/ospreferences;1"].getService(
        Ci.mozIOSPreferences
      ).systemLocale;
    } catch (e) {
      return null;
    }
  }
  
  //OS の言語設定への自動追従（この if に使用される pref は初回起動時のみ操作されます。この中にコードを書くと初回起動時のみ動作します）
  if (!Services.prefs.prefHasUserValue("intl.locale.requested")) {
    const systemlocale = getSystemLocale()
    Services.prefs.setStringPref("intl.locale.requested", systemlocale)



  //userChrome.css 等のスタートアップ時の処理 
  let userChromecssPath = OS.Path.join(OS.Constants.Path.profileDir, "chrome");
  //Linux, this is generally "$HOME/.floorp/Profiles/$PROFILENAME/chrome
  //Windows, this is generally "%APPDATA%\Local\temp\%PROFILENAME%"\chrome
  
  let uccpth = OS.Path.join(userChromecssPath, 'userChrome.css')
  OS.File.open(uccpth, {write: true, append: true}).then(valOpen => {
      var txtToAppend = `
      /*************************************************************************************************************************************************************************************************************************************************************
  
      userChrome.cssは、スタイルシートであり、Floorp のユーザーインターフェースに適用され、デフォルトの Floorp のスタイルルールをオーバーライドできます。 残念ながら、userChrome.cssを使用して Floorp の機能操作を変更することはできません。
  
      userChrome.cssファイルを作成し、スタイルルールを追加すると、フォントや色を変更したり、不要なアイテムを非表示にしたり、間隔を調整したり、Firefoxの外観を変更したりできます。
  
      タブバーを削除する
      ******************************************
      #tabbrowser-tabs{
        display: none;
      }
  
      ******************************************
      このように、要素の非表示などを行うことができます。自分が気に入らない要素を非表示にできるので、とても便利です。ネットにはこの実装例が公開されていることがあります。
  
      また、userChrome.css はブラウザーのツールバーに適用する CSS のことを指し、userContent.css はブラウザー内部サイトに対して CSS を適用できます。
      詳しくは、同じディレクトリに存在するファイルを参照してください
      
      NOTE:適用に、about:config の操作は不要です。
  
      参考: https://userChrome.org | https://github.com/topics/userchrome 
  
      ************************************************************************************************************************************************************************************************************************************************************/
  
      @charset "UTF-8";
      @-moz-document url(chrome://browser/content/browser.xhtml) {
      /*この下にCSSを書いてください*/
  
  
      }
      `;
  
      var txtEncoded = new TextEncoder().encode(txtToAppend);
      valOpen.write(txtEncoded)
      valOpen.close()
  });
  
  let ucconpth = OS.Path.join(userChromecssPath, 'userContent.css')
  OS.File.open(ucconpth, {write: true, append: true}).then(valOpen => {
    var txtToAppend = `
    /*************************************************************************************************************************************************************************************************************************************************************
     
    userContent.css は userChrome.css と同じく、chrome 特権を用いてブラウザーに対して CSS スタイルルールを指定できる特殊なCSSファイルです。
    ただし、userChrome.css と適用範囲はことなるので正しく理解しておく必要があります。
    
    userChrome.css は、ツールバーなどのブラウザーを制御する場所に適用するのに対し、userContent.css はブラウザー内部サイトにスタイルルールを定義できます。ただし、指定先をただしくURLで指定する必要があります。
  
    新しいタブに CSS を書く場合
    ***********************************
    @-moz-document url-prefix("about:newtab"), url-prefix("about:home") {
      
    /*ここに CSS を書いていく*/
  
    }
    ***********************************
  
    以上です。後の使い方はuserChrome.css と変わりません。Floorp をお楽しみください。
    
  
    ************************************************************************************************************************************************************************************************************************************************************/
  
    @charset "UTF-8";
  
    `;
    var txtEncoded = new TextEncoder().encode(txtToAppend);
    valOpen.write(txtEncoded)
    valOpen.close()
  });
  
  }

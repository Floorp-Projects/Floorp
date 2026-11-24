// SPDX-License-Identifier: MPL-2.0

// Search aliases are code-owned data, outside the translation sync targets.
// These readings restore the Japanese aliases removed by the translation sync
// in a8a45e006e11c04ed561990a5b30d853ceb9249b.
export const JAPANESE_COMMAND_READINGS: Readonly<
  Record<string, readonly string[]>
> = {
  "gecko-back": [
    "もどる",
  ],
  "gecko-forward": [
    "すすむ",
  ],
  "gecko-reload": [
    "さいよみこみ",
  ],
  "gecko-force-reload": [
    "きょうせいてきにさいよみこみ",
  ],
  "gecko-close-tab": [
    "たぶをとじる",
  ],
  "gecko-close-other-tabs": [
    "ほかのたぶをとじる",
  ],
  "gecko-close-tabs-to-start": [
    "ひだりのたぶをとじる",
  ],
  "gecko-close-tabs-to-end": [
    "みぎのたぶをとじる",
  ],
  "gecko-open-new-tab": [
    "あたらしたぶ",
  ],
  "gecko-duplicate-tab": [
    "たぶをふくせい",
  ],
  "gecko-reload-all-tabs": [
    "すべてのたぶをさいよみこみ",
  ],
  "gecko-restore-last-tab": [
    "とじたたぶをひらきなおす",
  ],
  "gecko-show-next-tab": [
    "つぎのたぶをひょうじ",
  ],
  "gecko-show-previous-tab": [
    "まえのたぶをひょうじ",
  ],
  "gecko-show-previously-selected-tab": [
    "まえにせんたくしたたぶをひょうじ",
  ],
  "gecko-show-all-tabs-panel": [
    "たぶをいちらんひょうじ",
  ],
  "gecko-mute-current-tab": [
    "げんざいのたぶをみゅーと",
  ],
  "gecko-zoom-in": [
    "かくだい",
  ],
  "gecko-zoom-out": [
    "しゅくしょう",
  ],
  "gecko-reset-zoom": [
    "かくだいりつをりせっと",
  ],
  "gecko-bookmark-this-page": [
    "このぺーじをぶっくまーく",
  ],
  "gecko-open-home-page": [
    "ほーむぺーじをひらく",
  ],
  "gecko-save-page": [
    "ぺーじをほぞん",
  ],
  "gecko-print-page": [
    "ぺーじをいんさつ",
  ],
  "gecko-show-source-of-page": [
    "ぺーじのそーすをひょうじ",
  ],
  "gecko-show-page-info": [
    "ぺーじのじょうほうをひょうじ",
  ],
  "gecko-open-screen-capture": [
    "すくりーんしょっとつーるをひらく",
  ],
  "gecko-search-in-this-page": [
    "ぺーじないをけんさく",
  ],
  "gecko-show-next-search-result": [
    "つぎのけんさくけっかをひょうじ",
  ],
  "gecko-show-previous-search-result": [
    "まえのけんさくけっかをひょうじ",
  ],
  "gecko-search-the-web": [
    "うぇぶをけんさく",
  ],
  "gecko-open-addons-manager": [
    "あどおんまねーじゃーをひらく",
  ],
  "gecko-send-with-mail": [
    "めーるでそうしん",
  ],
  "gecko-open-migration-wizard": [
    "いこううぃざーどをひらく",
  ],
  "gecko-quit-from-application": [
    "あぷりをしゅうりょう",
  ],
  "gecko-enter-into-customize-mode": [
    "かすたまいずもーどにはいる",
  ],
  "gecko-enter-into-offline-mode": [
    "おふらいんもーどにはいる",
  ],
  "gecko-open-task-manager": [
    "たすくまねーじゃーをひらく",
  ],
  "gecko-open-sync-preferences": [
    "どうきせっていをひらく",
  ],
  "gecko-open-downloads": [
    "だうんろーどをひらく",
  ],
  "gecko-forget-history": [
    "れきしをわすれる",
  ],
  "gecko-quick-forget-history": [
    "れきしをすぐにしょうきょ",
  ],
  "gecko-restore-last-session": [
    "ぜんかいのせっしょんをふくげん",
  ],
  "gecko-search-history": [
    "れきしをけんさく",
  ],
  "gecko-manage-history": [
    "れきしをかんり",
  ],
  "gecko-show-bookmark-sidebar": [
    "ぶっくまーくさいどばーをひょうじ",
  ],
  "gecko-show-history-sidebar": [
    "れきしさいどばーをひょうじ",
  ],
  "gecko-show-synced-tabs-sidebar": [
    "どうきたぶさいどばーをひょうじ",
  ],
  "gecko-reverse-sidebar": [
    "さいどばーのいちをはんてん",
  ],
  "gecko-hide-sidebar": [
    "さいどばーをかくす",
  ],
  "gecko-toggle-sidebar": [
    "さいどばーのひょうじをきりかえる",
  ],
  "gecko-scroll-up": [
    "うえにすくろーる",
  ],
  "gecko-scroll-down": [
    "したにすくろーる",
  ],
  "gecko-scroll-right": [
    "みぎにすくろーる",
  ],
  "gecko-scroll-left": [
    "ひだりにすくろーる",
  ],
  "gecko-scroll-to-top": [
    "さいじょうぶまですくろーる",
  ],
  "gecko-scroll-to-bottom": [
    "さいかぶまですくろーる",
  ],
  "gecko-workspace-next": [
    "つぎのわーくすぺーすにきりかえる",
  ],
  "gecko-workspace-previous": [
    "まえのわーくすぺーすにきりかえる",
  ],
  "floorp-rest-mode": [
    "きゅうけいもーどをゆうこうにする",
  ],
  "floorp-hide-user-interface": [
    "いんたーふぇーすをかくす",
  ],
  "floorp-toggle-navigation-panel": [
    "なびげーしょんぱねるのひょうじをきりかえる",
  ],
  "floorp-toggle-zen-mode": [
    "ぜんもーどのひょうじをきりかえる",
  ],
  "floorp-toggle-command-palette": [
    "こまんどぱれっとのひょうじをきりかえる",
  ],
  "floorp-open-settings": [
    "せっていをひらく",
  ],
  "floorp-open-hub": [
    "ふろあぱはぶをひらく",
  ],
  "floorp-show-pip": [
    "ぴくちゃーいんぴくちゃーをひょうじ",
  ],
  "floorp-toggle-share-mode": [
    "きょうゆうもーどのひょうじをきりかえる",
  ],
  "floorp-copy-page-url-as-markdown": [
    "ぺーじURLをまーくだうんでこぴー",
  ],
  "floorp-closed-tab-switcher": [
    "とじたたぶをひらきなおす",
  ],
  "floorp-closed-window-switcher": [
    "とじたうぃんどうをひらきなおす",
  ],
  "floorp-history-switcher": [
    "れきしをひらく",
  ],
  "floorp-bookmark-switcher": [
    "ぶっくまーくをひらく",
  ],
  "floorp-tab-switcher": [
    "たぶをきりかえる",
  ],
  "floorp-open-url": [
    "URLをひらく",
  ],
  "floorp-search-web": [
    "うぇぶをけんさく",
  ],
  "floorp-reopen-in-container": [
    "こんてなでひらきなおす",
  ],
};

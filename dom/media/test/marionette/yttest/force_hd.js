// This parts forces the highest definition
// https://addons.mozilla.org/en-US/firefox/addon/youtube-auto-hd-lq/
// licence: MPL 2.0
var config = {
  "HD": true,
  "LQ": false,
  "ID": "auto-hd-lq-for-ytb",
  "type": function (t) {
    config.HD = t === 'hd';
    config.LQ = t === 'lq';
  },
  "quality": function () {
    if (config.HD || config.LQ) {
      var youtubePlayerListener = function (LQ, HD) {
        return function youtubePlayerListener (e) {
          if (e === 1) {
            var player = document.getElementById('movie_player');
            if (player) {
              var levels = player.getAvailableQualityLevels();
              if (levels.length) {
                var q = (HD && levels[0]) ? levels[0] : ((LQ && levels[levels.length - 2]) ? levels[levels.length - 2] : null);
                if (q) {
                  player.setPlaybackQuality(q);
                  player.setPlaybackQualityRange(q, q);
                }
              }
            }
          }
        }
      }
      /*  */
      var inject = function () {
        var action = function () {
          var player = document.getElementById('movie_player');
          if (player && player.addEventListener && player.getPlayerState) {
            player.addEventListener("onStateChange", "youtubePlayerListener");
          } else window.setTimeout(action, 1000);
        };
        /*  */
        action();
      };
      var script = document.getElementById(config.ID);
      if (!script) {
        script = document.createElement("script");
        script.setAttribute("type", "text/javascript");
        script.setAttribute("id", config.ID);
        document.documentElement.appendChild(script);
      }
      /*  */
      script.textContent = "var youtubePlayerListener = (" + youtubePlayerListener + ')(' + config.LQ + ',' + config.HD  + ');(' + inject + ')();';
    }
  }
};

  if (/^https?:\/\/www\.youtube.com\/watch\?/.test(document.location.href)) config.quality();
  var content = document.getElementById('content');
  if (content) {
    var observer = new window.MutationObserver(function (e) {
      e.forEach(function (m) {
        if (m.addedNodes !== null) {
          for (var i = 0; i < m.addedNodes.length; i++) {
            if (m.addedNodes[i].id === 'movie_player') {
              config.quality();
              return;
            }
          }
        }
      });
    });
    /*  */
    observer.observe(content, {"childList": true, "subtree": true});
  }


# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

connection-window =
    .title = Connection Settings
    .style =
        { PLATFORM() ->
            [macos] width: 44em
           *[other] width: 49em
        }

connection-close-key =
    .key = w

connection-disable-extension =
    .label = Disable Extension

connection-proxy-configure = Configure Proxy Access to the Internet

connection-proxy-option-no =
    .label = No proxy
    .accesskey = y
connection-proxy-option-system =
    .label = Use system proxy settings
    .accesskey = U
connection-proxy-option-auto =
    .label = Auto-detect proxy settings for this network
    .accesskey = w
connection-proxy-option-manual =
    .label = Manual proxy configuration
    .accesskey = M

connection-proxy-http = HTTP Proxy
    .accesskey = x
connection-proxy-http-port = Port
    .accesskey = P
connection-proxy-https-sharing =
    .label = Also use this proxy for HTTPS
    .accesskey = s

connection-proxy-https = HTTPS Proxy
    .accesskey = H
connection-proxy-ssl-port = Port
    .accesskey = o

connection-proxy-socks = SOCKS Host
    .accesskey = C
connection-proxy-socks-port = Port
    .accesskey = t

connection-proxy-socks4 =
    .label = SOCKS v4
    .accesskey = K
connection-proxy-socks5 =
    .label = SOCKS v5
    .accesskey = v
connection-proxy-noproxy = No proxy for
    .accesskey = N

connection-proxy-noproxy-desc = Example: .mozilla.org, .net.nz, 192.168.1.0/24

# Do not translate "localhost", "127.0.0.1/8" and "::1". (You can translate "and".)
connection-proxy-noproxy-localhost-desc-2 = Connections to localhost, 127.0.0.1/8, and ::1 are never proxied.

connection-proxy-autotype =
    .label = Automatic proxy configuration URL
    .accesskey = A

connection-proxy-reload =
    .label = Reload
    .accesskey = e

connection-proxy-autologin =
    .label = Do not prompt for authentication if password is saved
    .accesskey = i
    .tooltip = This option silently authenticates you to proxies when you have saved credentials for them. You will be prompted if authentication fails.

connection-proxy-socks-remote-dns =
    .label = Proxy DNS when using SOCKS v5
    .accesskey = D

connection-dns-over-https =
    .label = Enable DNS over HTTPS
    .accesskey = b

connection-dns-over-https-url-resolver = Use Provider
    .accesskey = P

# Variables:
#   $name (String) - Display name or URL for the DNS over HTTPS provider
connection-dns-over-https-url-item-default =
    .label = { $name } (Default)
    .tooltiptext = Use the default URL for resolving DNS over HTTPS

connection-dns-over-https-url-custom =
    .label = Custom
    .accesskey = C
    .tooltiptext = Enter your preferred URL for resolving DNS over HTTPS

connection-dns-over-https-custom-label = Custom

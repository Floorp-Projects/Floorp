import time

import six
from mod_pywebsocket import msgutil

# see the list of tests in test_websocket.html


def web_socket_do_extra_handshake(request):
    # must set request.ws_protocol to the selected version from ws_requested_protocols
    for x in request.ws_requested_protocols:
        if x != "test-does-not-exist":
            request.ws_protocol = x
            break

    if request.ws_protocol == "test-2.1":
        time.sleep(3)
    elif request.ws_protocol == "test-9":
        time.sleep(3)
    elif request.ws_protocol == "test-10":
        time.sleep(3)
    elif request.ws_protocol == "test-19":
        raise ValueError("Aborting (test-19)")
    elif request.ws_protocol == "test-20" or request.ws_protocol == "test-17":
        time.sleep(3)
    elif request.ws_protocol == "test-22":
        # The timeout is 5 seconds
        time.sleep(13)
    elif request.ws_protocol == "test-41b":
        request.sts = "max-age=100"
    elif request.ws_protocol == "test-49":
        # subprotocols are compared case-sensitively, so this should fail
        request.ws_protocol = "teST-49"
    else:
        pass


# Behave according to recommendation of RFC 6455, section # 5.5.1:
#  "When sending a Close frame in response, the endpoint typically echos the
#   status code it received."
# - Without this, pywebsocket replies with 1000 to any close code.
#
#  Note that this function is only called when the client initiates the close


def web_socket_passive_closing_handshake(request):
    if request.ws_close_code == 1005:
        return None, None
    return request.ws_close_code, request.ws_close_reason


def web_socket_transfer_data(request):
    if request.ws_protocol == "test-1" or request.ws_protocol == "test-4":
        msgutil.send_message(request, "server data")
        msgutil.close_connection(request)
    elif request.ws_protocol == "test-2.1" or request.ws_protocol == "test-2.2":
        msgutil.close_connection(request)
    elif request.ws_protocol == "test-6":
        resp = "wrong message"
        if msgutil.receive_message(request) == "1":
            resp = "2"
        msgutil.send_message(request, six.ensure_text(resp))
        resp = "wrong message"
        if msgutil.receive_message(request) == "3":
            resp = "4"
        msgutil.send_message(request, six.ensure_text(resp))
        resp = "wrong message"
        if msgutil.receive_message(request) == "5":
            resp = (
                b"\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a".decode(
                    "utf-8"
                )
            )
        msgutil.send_message(request, six.ensure_text(resp))
        msgutil.close_connection(request)
    elif request.ws_protocol == "test-7":
        msgutil.send_message(request, "test-7 data")
    elif request.ws_protocol == "test-10":
        msgutil.close_connection(request)
    elif request.ws_protocol == "test-11":
        resp = "wrong message"
        if msgutil.receive_message(request) == "client data":
            resp = "server data"
        msgutil.send_message(request, six.ensure_text(resp))
    elif request.ws_protocol == "test-12":
        msg = msgutil.receive_message(request)
        if msg == "a\ufffdb":
            # converted unpaired surrogate in UTF-16 to UTF-8 OK
            msgutil.send_message(request, "SUCCESS")
        else:
            msgutil.send_message(
                request,
                "FAIL got '" + msg + "' instead of string with replacement char'",
            )
    elif request.ws_protocol == "test-13":
        # first one binary message containing the byte 0x61 ('a')
        request.connection.write(b"\xff\x01\x61")
        # after a bad utf8 message
        request.connection.write(b"\x01\x61\xff")
        msgutil.close_connection(request)
    elif request.ws_protocol == "test-14":
        msgutil.close_connection(request)
        msgutil.send_message(request, "server data")
    elif request.ws_protocol == "test-15":
        # DISABLED: close_connection hasn't supported 2nd 'abort' argument for a
        # long time.  Passing extra arg was causing exception, which conveniently
        # caused abort :) but as of pywebsocket v606 raising an exception here no
        # longer aborts, and there's no obvious way to close TCP connection w/o
        # sending websocket CLOSE.
        raise RuntimeError("test-15 should be disabled for now")
        # msgutil.close_connection(request, True)   # OBSOLETE 2nd arg
        # return
    elif request.ws_protocol == "test-17" or request.ws_protocol == "test-21":
        time.sleep(2)
        resp = "wrong message"
        if msgutil.receive_message(request) == "client data":
            resp = "server data"
        msgutil.send_message(request, six.ensure_text(resp))
        time.sleep(2)
        msgutil.close_connection(request)
    elif request.ws_protocol == "test-20":
        msgutil.send_message(request, "server data")
        msgutil.close_connection(request)
    elif request.ws_protocol == "test-34":
        request.ws_stream.close_connection(1001, "going away now")
    elif request.ws_protocol == "test-35a":
        while not request.client_terminated:
            msgutil.receive_message(request)
        global test35code
        test35code = request.ws_close_code
        global test35reason
        test35reason = request.ws_close_reason
    elif request.ws_protocol == "test-35b":
        request.ws_stream.close_connection(test35code + 1, test35reason)
    elif request.ws_protocol == "test-37b":
        while not request.client_terminated:
            msgutil.receive_message(request)
        global test37code
        test37code = request.ws_close_code
        global test37reason
        test37reason = request.ws_close_reason
    elif request.ws_protocol == "test-37c":
        request.ws_stream.close_connection(test37code, test37reason)
    elif request.ws_protocol == "test-42":
        # Echo back 3 messages
        msgutil.send_message(request, msgutil.receive_message(request))
        msgutil.send_message(request, msgutil.receive_message(request))
        msgutil.send_message(request, msgutil.receive_message(request))
    elif request.ws_protocol == "test-44":
        rcv = six.ensure_text(msgutil.receive_message(request))
        # check we received correct binary msg
        if len(rcv) == 3 and ord(rcv[0]) == 5 and ord(rcv[1]) == 0 and ord(rcv[2]) == 7:
            # reply with binary msg 0x04
            msgutil.send_message(request, b"\x00\x04", True, True)
        else:
            msgutil.send_message(request, "incorrect binary msg received!")
    elif request.ws_protocol == "test-45":
        rcv = msgutil.receive_message(request)
        # check we received correct binary msg
        if six.ensure_text(rcv) == "flob":
            # send back same blob as binary msg
            msgutil.send_message(request, rcv, True, True)
        else:
            msgutil.send_message(
                request, "incorrect binary msg received: '" + rcv + "'"
            )
    elif request.ws_protocol == "test-46":
        msgutil.send_message(request, "client must drop this if close was called")

    while not request.client_terminated:
        msgutil.receive_message(request)

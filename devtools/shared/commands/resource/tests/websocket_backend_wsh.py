from __future__ import absolute_import
from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    while not request.client_terminated:
        resp = msgutil.receive_message(request)
        msgutil.send_message(request, resp)


def web_socket_passive_closing_handshake(request):
    # If we use `pass` here, the `payload` of `frameReceived` which will be happened
    # of communication of closing will be `\u0003è`. In order to make the `payload`
    # to be empty string, return code and reason explicitly.
    code = None
    reason = None
    return code, reason

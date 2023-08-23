from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    while not request.client_terminated:
        resp = msgutil.receive_message(request)
        msgutil.send_message(request, resp, binary=isinstance(resp, bytes))

    msgutil.close_connection(request)

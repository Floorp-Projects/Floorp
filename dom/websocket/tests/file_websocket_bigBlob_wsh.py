from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass


def web_socket_transfer_data(request):
    while True:
        line = msgutil.receive_message(request)
        msgutil.send_message(request, line, True, True)

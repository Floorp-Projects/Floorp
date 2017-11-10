from mod_pywebsocket import msgutil

def web_socket_do_extra_handshake(request):
  pass

def web_socket_transfer_data(request):
  resp = "Test"
  if msgutil.receive_message(request) == "data":
    resp = "Hello world!"
  msgutil.send_message(request, resp)

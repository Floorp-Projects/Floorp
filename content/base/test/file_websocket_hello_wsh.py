from mod_pywebsocket import msgutil

def web_socket_do_extra_handshake(request):
  pass

def web_socket_transfer_data(request):
  resp = "Fail"
  request_text = msgutil.receive_message(request)
  if request_text == "data":
    resp = "Hello world!"
  elif request_text == "ssltest":
    resp = "ssldata"
  msgutil.send_message(request, resp)

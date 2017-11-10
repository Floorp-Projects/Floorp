from mod_pywebsocket import msgutil

def web_socket_do_extra_handshake(request):
  # must set request.ws_protocol to the selected version from ws_requested_protocols
  request.ws_protocol = request.ws_requested_protocols[0]

  if (request.ws_protocol == 'error'):
      raise ValueError('Error')
  pass

def web_socket_transfer_data(request):
  while True:
    line = msgutil.receive_message(request)
    if line == 'protocol':
      msgutil.send_message(request, request.ws_protocol)
      continue

    if line == 'resource':
      msgutil.send_message(request, request.ws_resource)
      continue

    if line == 'origin':
      msgutil.send_message(request, request.ws_origin)
      continue

    msgutil.send_message(request, line)

    if line == 'end':
      return

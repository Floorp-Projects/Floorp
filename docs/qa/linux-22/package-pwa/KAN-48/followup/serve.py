#!/usr/bin/env python3
from pathlib import Path
import sys,http.server
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from pwa_ui_probe import Fixture
http.server.ThreadingHTTPServer(('127.0.0.1',int(sys.argv[1])),Fixture).serve_forever()

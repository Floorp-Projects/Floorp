FROM debian:stretch-20181112

COPY setup.sh /root
RUN cd /root && ./setup.sh

RUN useradd -d /home/worker -s /bin/bash -m worker
USER worker
WORKDIR /home/worker
CMD /bin/bash

COPY setup-standalone.sh /home/worker
RUN cd /home/worker && ./setup-standalone.sh

import React, { useEffect, useState } from "react";

export function Clock() {
  const [now, setNow] = useState(() => new Date());
  useEffect(() => {
    const timer = setInterval(() => setNow(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);
  return (
    <time className="home-clock" dateTime={now.toISOString()} aria-label={now.toLocaleString()}>
      <span className="clock-time">{now.toLocaleTimeString(undefined, { hour: "2-digit", minute: "2-digit", hour12: false })}</span>
      <span className="clock-date">{now.toLocaleDateString(undefined, { month: "long", day: "numeric", weekday: "short" })}</span>
    </time>
  );
}

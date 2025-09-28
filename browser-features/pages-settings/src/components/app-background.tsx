export function AppBackground() {
    return (
        <div className="fixed inset-0 pointer-events-none overflow-hidden">
            <div className="fixed inset-0 opacity-[0.03]"
                style={{
                    backgroundImage: 'linear-gradient(to right, currentColor 1px, transparent 1px), linear-gradient(to bottom, currentColor 1px, transparent 1px)',
                    backgroundSize: '40px 40px'
                }}>
            </div>

            <div className="fixed -right-20 -top-20 w-96 h-96 opacity-[0.07]">
                <div className="relative w-full h-full">
                    {[...Array(24)].map((_, i) => (
                        <div key={i} className="absolute"
                            style={{
                                width: '40px',
                                height: '35px',
                                transform: `rotate(${i * 15}deg) translateY(-120px)`,
                                transformOrigin: 'center center',
                            }}>
                            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" className="text-primary">
                                <path d="M12,2L2,7.5L2,16.5L12,22L22,16.5L22,7.5L12,2Z" strokeWidth="1" />
                            </svg>
                        </div>
                    ))}
                </div>
            </div>

            <div className="fixed -left-10 bottom-1/4 opacity-[0.06]">
                <div className="grid grid-cols-5 gap-6">
                    {[...Array(25)].map((_, i) => (
                        <div key={i} className={`w-10 h-10 ${i % 2 === 0 ? 'rotate-0' : 'rotate-180'}`}>
                            <svg viewBox="0 0 24 24" fill="currentColor" className="text-secondary w-full h-full">
                                <path d="M1,21H23L12,2" />
                            </svg>
                        </div>
                    ))}
                </div>
            </div>

            <div className="fixed right-1/4 -bottom-20 opacity-[0.06]">
                <div className="grid grid-cols-4 grid-rows-4 gap-4">
                    {[...Array(16)].map((_, i) => (
                        <div key={i} className={`w-8 h-8 rounded-full border-2 border-accent ${i % 3 === 0 ? 'bg-accent/20' : ''}`}></div>
                    ))}
                </div>
            </div>

            <div className="fixed left-0 top-1/3 opacity-[0.05]">
                <div className="grid grid-cols-3 gap-2">
                    {[...Array(9)].map((_, i) => (
                        <div key={i} className={`w-12 h-12 border border-primary/40 ${i % 2 === 0 ? 'rotate-45' : ''}`}></div>
                    ))}
                </div>
            </div>

            <div className="fixed right-20 top-1/3 w-48 h-64 opacity-[0.04]"
                style={{
                    backgroundImage: 'repeating-linear-gradient(45deg, currentColor, currentColor 1px, transparent 1px, transparent 10px)',
                }}>
            </div>

            <div className="fixed left-1/4 top-1/2 transform -translate-y-1/2 opacity-[0.08]">
                <div className="grid grid-cols-10 gap-3">
                    {[...Array(100)].map((_, i) => (
                        <div key={i} className={`w-1 h-1 rounded-full ${i % 5 === 0 ? 'bg-primary' : i % 3 === 0 ? 'bg-secondary' : 'bg-accent'}`}></div>
                    ))}
                </div>
            </div>
        </div>
    );
}